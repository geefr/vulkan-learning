
#include "registrar.h"
#include "util.h"

#include <iostream>
#include <fstream>

Registrar Registrar::mReg;

Registrar::Registrar()
{
}

Registrar::~Registrar()
{

}

void Registrar::tearDown() {

  // TODO: Not sure if all this is needed?
  // Initially thought the smart pointers would
  // be best but might be hard to sort out
  // cleanup order in that case..
  mDeviceInstance->waitAllDevicesIdle();

  mFrameBuffer.release();
  mGraphicsPipeline.release();
  mPipelineLayout.release();
  mRenderPass.release();
  mWindowIntegration.release();
  for( auto& p : mCommandPools ) p.release();
  Util::release();

  mDeviceInstance.release();
}


vk::CommandPool& Registrar::createCommandPool( vk::CommandPoolCreateFlags flags ) {
  vk::CommandPoolCreateInfo info(flags, mDeviceInstance->queueFamilyIndex()); // TODO: Hack! should be passed by caller/some reference to the command queue class we don't have yet
  mCommandPools.emplace_back(mDeviceInstance->device().createCommandPoolUnique(info));
  return mCommandPools.back().get();
}

vk::UniqueBuffer Registrar::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags ) {
  // Exclusive buffer of size, for usageFlags
  vk::BufferCreateInfo info(
        vk::BufferCreateFlags(),
        size,
        usageFlags
        );
  return mDeviceInstance->device().createBufferUnique(info);
}

/// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
uint32_t Registrar::selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags ) {
  // Initial implementation doesn't have any real requirements, just select the first compatible heap
  vk::PhysicalDeviceMemoryProperties memoryProperties = mDeviceInstance->physicalDevice().getMemoryProperties();

  for(uint32_t memType = 0u; memType < 32; ++memType) {
    uint32_t memTypeBit = 1 << memType;
    if( memoryRequirements.memoryTypeBits & memTypeBit ) {
      auto deviceMemType = memoryProperties.memoryTypes[memType];
      if( (deviceMemType.propertyFlags & requiredFlags ) == requiredFlags ) {
        return memType;
      }
    }
  }
  throw std::runtime_error("Failed to find suitable heap type for flags: " + vk::to_string(requiredFlags));
}

/// Allocate device memory suitable for the specified buffer
vk::UniqueDeviceMemory Registrar::allocateDeviceMemoryForBuffer( vk::Buffer& buffer, vk::MemoryPropertyFlags userReqs ) {
  // Find out what kind of memory the buffer needs
  vk::MemoryRequirements memReq = mDeviceInstance->device().getBufferMemoryRequirements(buffer);

  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );

  vk::MemoryAllocateInfo info(
        memReq.size,
        heapIdx);

  return mDeviceInstance->device().allocateMemoryUnique(info);
}

/// Bind memory to a buffer
void Registrar::bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset) {
  mDeviceInstance->device().bindBufferMemory(buffer, memory, offset);
}

/// Map a region of device memory to host memory
void* Registrar::mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size ) {
  return mDeviceInstance->device().mapMemory(deviceMem, offset, size);
}

/// Unmap a region of device memory
void Registrar::unmapMemory( vk::DeviceMemory& deviceMem ) {
  mDeviceInstance->device().unmapMemory(deviceMem);
}

/// Flush memory/caches
void Registrar::flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem ) {
  mDeviceInstance->device().flushMappedMemoryRanges(mem.size(), mem.data());
}


void Registrar::createRenderPass() {
  // The render pass contains stuff like what the framebuffer attachments are and things
  vk::AttachmentDescription colourAttachment = {};
  colourAttachment.setFormat(mWindowIntegration->format())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eClear) // What to do before rendering
      .setStoreOp(vk::AttachmentStoreOp::eStore) // What to do after rendering
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined) // Image layout before render pass begins, we don't care, gonna clear anyway
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR) // Image layout to transfer to when render pass finishes, ready for presentation
      ;

  // For starters we only need one sub-pass to draw
  // Multiple sub passes are used for multi-pass rendering
  // putting them in one overall render pass means vulkan can reorder
  // them as needed
  vk::AttachmentReference colourAttachmentRef = {};
  colourAttachmentRef.setAttachment(0) // Index in attachment description array (We only have one so far)
      .setLayout(vk::ImageLayout::eColorAttachmentOptimal) // Layout for the attachment during this subpass. This attachment is our colour buffer so marked as such here
      ;

  vk::SubpassDescription subpass = {};
  subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics) // Marked as a graphics subpass, looks like we can mix multiple subpass types in one render pass?
      .setColorAttachmentCount(1)
      .setPColorAttachments(&colourAttachmentRef)
      ;
  // Index of colour attachment here matches layout = 0 in fragment shader
  // So the frag shader can reference multiple attachments as its output!

  // Now let's actually make the render pass
  vk::RenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.setAttachmentCount(1)
      .setPAttachments(&colourAttachment)
      .setSubpassCount(1)
      .setPSubpasses(&subpass)
      ;

  /*
   * Setup subpass dependencies
   * This is needed to ensure that the render pass
   * doesn't begin until tthe image is available
   *
   * Apparently an alternative is to set the waitStages of the command buffer
   * to top of pipe to ensure rendering doesn't begin until the pipeline is started?
   * https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation
   *
   * TODO: Yeah I don't understand this and the tutorials gloss over it, probably more info in the
   * synchronisation chapter of the book
   */

  vk::SubpassDependency dep = {};
  dep.setSrcSubpass(VK_SUBPASS_EXTERNAL) // implicit subpass before this pass
      .setDstSubpass(0) // The index of our subpass
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) // Wait on this operation
      .setSrcAccessMask(vk::AccessFlags()) // Which needs to happen in this stage...here we're waiting for the swap chain to finish reading from the image
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) // The operations which should wait are in the colour attachment stage
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite) // and involve the reading and writing of the colour attachment
                                                            // These prevent the transition from happening until it's necessary/allowed, when we want to start writing colours to it..
      ;

  renderPassInfo.setDependencyCount(1)
      .setPDependencies(&dep)
      ;

  mRenderPass = mDeviceInstance->device().createRenderPassUnique(renderPassInfo);
}

void Registrar::createGraphicsPipeline() {
  /*
   * The programmable parts of the pipeline are similar to gl
   * And in this case the shaders were even compiled from glsl -> SPIR-V
   */
  auto vertShaderMod = createShaderModule("vert.spv");
  vk::PipelineShaderStageCreateInfo vertInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eVertex,
        vertShaderMod.get(),
        "main",
        nullptr // Specialisation info - allows specification of shader constants at link time
        );

  auto fragShaderMod = createShaderModule("frag.spv");
  vk::PipelineShaderStageCreateInfo fragInfo(
        vk::PipelineShaderStageCreateFlags(),
        vk::ShaderStageFlagBits::eFragment,
        fragShaderMod.get(),
        "main",
        nullptr // Specialisation info - allows specification of shader constants at link time
        );

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertInfo, fragInfo};

  /*
   * Then there's the fixed function sections of the pipeline
   */
  // Vertex input
  // For now nothing here as vertices are hardcoded in the shader
  vk::PipelineVertexInputStateCreateInfo vertInputInfo(
        {},
        0, //Binding description count
        nullptr, // Binding descriptions
        0, // Attribute description count
        nullptr // Attribute descriptions
        );

  // Input assembly
  // Typical triangles setup
  vk::PipelineInputAssemblyStateCreateInfo inputAssInfo(
        {},
        vk::PrimitiveTopology::eTriangleList,
        false // Primitive restart
        );

  // Viewport
  vk::Viewport viewport(0.f,0.f, mWindowIntegration->extent().width, mWindowIntegration->extent().height, 0.f, 1.f);
  vk::Rect2D scissor({0,0},mWindowIntegration->extent());
  vk::PipelineViewportStateCreateInfo viewportInfo({}, 1, &viewport, 1, &scissor);

  // Rasteriser
  vk::PipelineRasterizationStateCreateInfo rasterisationInfo(
          {},
          false, // Depth clamp enable
          false, // Discard enable
          vk::PolygonMode::eFill, // Cool, we can just toggle this to make a wireframe <3
          vk::CullModeFlagBits::eBack, // Backface culling of course
          vk::FrontFace::eCounterClockwise, // Tutorial is of course wrong here, so we'll need to fix the vertices
          false, // Depth bias enable
          0.f, // Depth bias factor
          false, // Depth bias clamp
          0.f, // Depth bias clamp
          1.f // Line width
          );

  // Multisampling
  vk::PipelineMultisampleStateCreateInfo multisampleInfo = {};
  multisampleInfo.setSampleShadingEnable(false)
                .setRasterizationSamples(vk::SampleCountFlagBits::e1)
                ;

  // Depth/Stencil test
  // Not yet, we're gonna pass null for this one ;)

  // Colour blending
  // attachment state is per-framebuffer
  // creat info is global to the pipeline
  vk::PipelineColorBlendAttachmentState colourBlendAttach = {};
  colourBlendAttach.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
      .setBlendEnable(false)
      /*
      .setBlendEnable(true)
      .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .setDstColorBlendFactor(vk::BlendFactor::eOneMinusDstAlpha)
      .setColorBlendOp(vk::BlendOp::eAdd)
      .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
      .setAlphaBlendOp(vk::BlendOp::eAdd)
      */
      ;

  vk::PipelineColorBlendStateCreateInfo colourBlendInfo = {};
  colourBlendInfo.setLogicOpEnable(false)
      .setLogicOp(vk::LogicOp::eCopy)
      .setAttachmentCount(1)
      .setPAttachments(&colourBlendAttach)
      .setBlendConstants({0.f,0.f,0.f,0.f})
      ;

  // Dynamic state
  // There's a dynamic state section that allows setting things like viewport/etc without rebuilding
  // the entire pipeline
  // If this is provided then the state has to be provided at draw time, so for now it's just another nullptr

  // Pipeline layout
  // Pipeline layout is where uniforms and such go
  // and they have to be known when the pipeline is built
  // So no randomly chucking uniforms around like we do in gl right?
  vk::PipelineLayoutCreateInfo layoutInfo = {};
  layoutInfo.setSetLayoutCount(0)
      .setPSetLayouts(nullptr)
      .setPushConstantRangeCount(0)
      .setPPushConstantRanges(nullptr)
      ;

  mPipelineLayout = mDeviceInstance->device().createPipelineLayoutUnique(layoutInfo);


  // Finally let's make the pipeline itself
  vk::GraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.setStageCount(2)
      .setPStages(shaderStages)
      .setPVertexInputState(&vertInputInfo)
      .setPInputAssemblyState(&inputAssInfo)
      .setPViewportState(&viewportInfo)
      .setPRasterizationState(&rasterisationInfo)
      .setPMultisampleState(&multisampleInfo)
      .setPDepthStencilState(nullptr)
      .setPColorBlendState(&colourBlendInfo)
      .setPDynamicState(nullptr)
      .setLayout(mPipelineLayout.get())
      .setRenderPass(mRenderPass.get()) // The render pass the pipeline will be used in
      .setSubpass(0) // The sub pass the pipeline will be used in
      .setBasePipelineHandle({}) // Derive from an existing pipeline
      .setBasePipelineIndex(-1) // Or the index of a pipeline, which may not yet exist. DERIVATIVE_BIT must be specified in flags to do this
      ;


  mGraphicsPipeline = mDeviceInstance->device().createGraphicsPipelineUnique({}, pipelineInfo);

  // Shader modules deleted here, only needed for pipeline init
}


std::vector<char> Registrar::readFile(const std::string& fileName) {
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);
  if( !file.is_open() ) throw std::runtime_error("Failed to open file: " + fileName);
  auto fileSize = file.tellg(); // We started at the end
  std::vector<char> buf(fileSize);
  file.seekg(0);
  file.read(buf.data(), fileSize);
  file.close();
  return buf;
}

vk::UniqueShaderModule Registrar::createShaderModule(const std::string& fileName) {
  auto shaderCode = readFile(fileName);
  // Note that data passed to info is as uint32_t*, so must be 4-byte aligned
  // According to tutorial std::vector already satisfies worst case alignment needs
  // TODO: But we should probably double check the alignment here just incase
  vk::ShaderModuleCreateInfo info(
        vk::ShaderModuleCreateFlags(),
        shaderCode.size(),
        reinterpret_cast<const uint32_t*>(shaderCode.data())
        );
  return mDeviceInstance->device().createShaderModuleUnique(info);
}
