
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
  mDevice->waitIdle();

  for( auto& f : mSwapChainFrameBuffers ) f.release(); // Before image views and render pass
  mGraphicsPipeline.release();
  mPipelineLayout.release();
  mRenderPass.release();
  for( auto& iv : mSwapChainImageViews ) iv.release();
  mSwapChain.release();
  mSurface.release();
  for( auto& p : mCommandPools ) p.release();
  mDevice.release();

  Util::release();

  mInstance.release();

}


vk::Instance& Registrar::createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer) {
  // First let's validate whether the extensions we need are available
  // If not there's no point doing anything and the system can't support the program
  // Assume that if presentation extensions are enabled at compile time then they're
  // required
  // TODO: If there's multiple possibilities such as on Linux then maybe we need something more complicated
  auto supportedExtensions = vk::enumerateInstanceExtensionProperties();

  std::vector<const char*> enabledInstanceExtensions = requiredExtensions;
#ifdef VK_USE_PLATFORM_WIN32_KHR
  ensureInstanceExtension(supportedExtensions, "TODO");
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
  enabledInstanceExtensions.push_back("VK_KHR_surface");
  enabledInstanceExtensions.push_back("VK_KHR_xlib_surface");

#endif
#ifdef VK_USE_PLATFORM_LIB_XCB_KHR
  ensureInstanceExtension(supportedExtensions, "TODO");
#endif


  vk::ApplicationInfo applicationInfo(
        appName.c_str(), // Application Name
        appVer, // Application version
        "geefr", // engine name
        1, // engine version
        apiVer // absolute minimum vulkan api
        );

#ifdef DEBUG
  uint32_t enabledLayerCount = 1;
  const char* const enabledLayerNames[] = {
    "VK_LAYER_LUNARG_standard_validation",
  };
  enabledInstanceExtensions.push_back("VK_EXT_debug_utils");

#else
  uint32_t enabledLayerCount = 0;
  const char* const* enabledLayerNames = nullptr;
#endif

  for( auto& e : enabledInstanceExtensions ) Util::ensureExtension(supportedExtensions, e);
  vk::InstanceCreateInfo instanceCreateInfo(
        vk::InstanceCreateFlags(),
        &applicationInfo, // Application Info
        enabledLayerCount, // enabledLayerCount - don't need any validation layers right now
        enabledLayerNames, // ppEnabledLayerNames
        static_cast<uint32_t>(enabledInstanceExtensions.size()), // enabledExtensionCount. In vulkan these need to be requested on init unlike in gl
        enabledInstanceExtensions.data() // ppEnabledExtensionNames
        );

  mInstance = vk::createInstanceUnique(instanceCreateInfo);

#ifdef DEBUG
  Util::initDidl(mInstance.get());
  Util::initDebugMessenger(mInstance.get());
#endif

  mPhysicalDevices = mInstance->enumeratePhysicalDevices();
  if( mPhysicalDevices.empty() ) throw std::runtime_error("Failed to enumerate physical devices");

  // Device order in the list isn't guaranteed, likely the integrated gpu is first
  // So why don't we fix that? As we're using the C++ api we can just sort the vector
  std::sort(mPhysicalDevices.begin(), mPhysicalDevices.end(), [&](auto& a, auto& b) {
    vk::PhysicalDeviceProperties propsA;
    a.getProperties(&propsA);
    vk::PhysicalDeviceProperties propsB;
    b.getProperties(&propsB);
    // For now just ensure discrete GPUs are first, then order by support vulkan api version
    if( propsA.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ) return true;
    if( propsA.apiVersion > propsB.apiVersion ) return true;
    return false;
  });

  return mInstance.get();
}

vk::Device& Registrar::createLogicalDevice(vk::QueueFlags qFlags ) {
  mQueueFamIndex = findQueue(mPhysicalDevices.front(), qFlags);
  return createLogicalDevice();
}

vk::Device& Registrar::createLogicalDevice() {
  if( mQueueFamIndex > mPhysicalDevices.size() ) throw std::runtime_error("createLogicalDevice: Physical device doesn't support required queue types");

  auto supportedExtensions = mPhysicalDevices.front().enumerateDeviceExtensionProperties();
  std::vector<const char*> enabledDeviceExtensions;
#if defined(VK_USE_PLATFORM_WIN32_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_LIB_XCB_KHR) || defined(USE_GLFW)
  enabledDeviceExtensions.push_back("VK_KHR_swapchain");
  for( auto& e : enabledDeviceExtensions ) Util::ensureExtension(supportedExtensions, e);
#endif

#ifdef DEBUG
  uint32_t enabledLayerCount = 1;
  const char* const enabledLayerNames[] = {
    "VK_LAYER_LUNARG_standard_validation",
  };

#else
  uint32_t enabledLayerCount = 0;
  const char* const* enabledLayerNames = nullptr;
#endif

  float queuePriorities = 1.f;
  vk::DeviceQueueCreateInfo queueInfo(
        vk::DeviceQueueCreateFlags(),
        mQueueFamIndex, // Queue family index
        1, // Number of queues to create
        &queuePriorities // Priorities of each queue
        );

  // The features of the physical device
  vk::PhysicalDeviceFeatures deviceSupportedFeatures;
  mPhysicalDevices.front().getFeatures(&deviceSupportedFeatures);

  // The features we require, we get very little without requesting these
  // As listed page 17
  // TODO: After creation the enabled features are set in this struct, will want to keep it for later
  vk::PhysicalDeviceFeatures deviceRequiredFeatures;
  deviceRequiredFeatures.multiDrawIndirect = deviceSupportedFeatures.multiDrawIndirect;
  deviceRequiredFeatures.tessellationShader = true;
  deviceRequiredFeatures.geometryShader = true;

  vk::DeviceCreateInfo info(
        vk::DeviceCreateFlags(),
        1, // Queue create info count
        &queueInfo, // Queue create info structs
        enabledLayerCount, // Enabled layer count
        enabledLayerNames, // Enabled layers
        static_cast<uint32_t>(enabledDeviceExtensions.size()), // Enabled extension count
        enabledDeviceExtensions.data(), // Enabled extensions
        &deviceRequiredFeatures // Physical device features
        );

  mDevice = mPhysicalDevices.front().createDeviceUnique(info);

  mQueues.emplace_back( mDevice->getQueue(mQueueFamIndex, 0) );

  return mDevice.get();
}

vk::CommandPool& Registrar::createCommandPool( vk::CommandPoolCreateFlags flags ) {
  vk::CommandPoolCreateInfo info(flags, mQueueFamIndex); // TODO: Hack! should be passed by caller/some reference to the command queue class we don't have yet
  mCommandPools.emplace_back(mDevice->createCommandPoolUnique(info));
  return mCommandPools.back().get();
}

vk::UniqueBuffer Registrar::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags ) {
  // Exclusive buffer of size, for usageFlags
  vk::BufferCreateInfo info(
        vk::BufferCreateFlags(),
        size,
        usageFlags
        );
  return mDevice->createBufferUnique(info);
}

/// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
uint32_t Registrar::selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags ) {
  // Initial implementation doesn't have any real requirements, just select the first compatible heap
  vk::PhysicalDeviceMemoryProperties memoryProperties = mPhysicalDevices.front().getMemoryProperties();

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
  vk::MemoryRequirements memReq = mDevice->getBufferMemoryRequirements(buffer);

  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );

  vk::MemoryAllocateInfo info(
        memReq.size,
        heapIdx);

  return mDevice->allocateMemoryUnique(info);
}

/// Bind memory to a buffer
void Registrar::bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset) {
  mDevice->bindBufferMemory(buffer, memory, offset);
}

/// Map a region of device memory to host memory
void* Registrar::mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size ) {
  return mDevice->mapMemory(deviceMem, offset, size);
}

/// Unmap a region of device memory
void Registrar::unmapMemory( vk::DeviceMemory& deviceMem ) {
  mDevice->unmapMemory(deviceMem);
}

/// Flush memory/caches
void Registrar::flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem ) {
  mDevice->flushMappedMemoryRanges(mem.size(), mem.data());
}

uint32_t Registrar::findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags) {
  auto qFamProps = device.getQueueFamilyProperties();
  auto it = std::find_if(qFamProps.begin(), qFamProps.end(), [&](auto& p) {
    if( p.queueFlags & requiredFlags ) return true;
    return false;
  });
  if( it == qFamProps.end() ) return std::numeric_limits<uint32_t>::max();
  return it - qFamProps.begin();
}

#ifdef VK_USE_PLATFORM_XLIB_KHR
uint32_t Registrar::findPresentQueueXlib(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags, Display* dpy, VisualID vid)
{
  auto qFamProps = device.getQueueFamilyProperties();

  uint32_t qIdx = std::numeric_limits<uint32_t>::max();
  for( auto i = 0u; i < qFamProps.size(); ++i ) {
    auto& p = qFamProps[i];
    if( !(p.queueFlags & requiredFlags) ) continue;
    if( device.getXlibPresentationSupportKHR(i, dpy, vid)) {
      qIdx = i;
      break;
    }
  }
  return qIdx;
}

vk::Device& Registrar::createLogicalDeviceWithPresentQueueXlib(vk::QueueFlags qFlags, Display* dpy, VisualID vid) {
  mQueueFamIndex = findPresentQueueXlib( mPhysicalDevices.front(), qFlags, dpy, vid);

  auto& result = createLogicalDevice();

  return result;
}

vk::SurfaceKHR& Registrar::createSurfaceXlib(Display* dpy, Window window) {
  vk::XlibSurfaceCreateInfoKHR info(
        vk::XlibSurfaceCreateFlagsKHR(),
        dpy,
        window
        );

  mSurface = mInstance->createXlibSurfaceKHRUnique(info);
  return mSurface.get();
}
#endif

#ifdef USE_GLFW
vk::SurfaceKHR& Registrar::createSurfaceGLFW(GLFWwindow* window) {
  VkSurfaceKHR surface;
  if(glfwCreateWindowSurface(mInstance.get(), window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("createSurfaceGLFW: Failed to create window surface");
  }

  mSurface.reset(surface);
  return mSurface.get();
}
#endif


vk::SwapchainKHR& Registrar::createSwapChain() {
  if( !mPhysicalDevices.front().getSurfaceSupportKHR(mQueueFamIndex, mSurface.get()) ) throw std::runtime_error("createSwapChainXlib: Physical device doesn't support surfaces");

  // Parameters used in swapchain must comply with limits of the surface
  auto caps = mPhysicalDevices.front().getSurfaceCapabilitiesKHR(mSurface.get());

  // First the number of images (none, double, triple buffered)
  auto numImages = 3u;
  while( numImages > caps.maxImageCount ) numImages--;
  if( numImages != 3u ) std::cerr << "Creating swapchain with " << numImages << " images" << std::endl;
  if( numImages < caps.minImageCount ) throw std::runtime_error("Unable to create swapchain, invalid num images");

  // Ideally we want full alpha support
  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied; // TODO: Pre or post? can't remember the difference
  if( !(caps.supportedCompositeAlpha & alphaMode) ) {
    std::cerr << "Surface doesn't support full alpha, falling back" << std::endl;
    alphaMode = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  }

  auto imageUsage = vk::ImageUsageFlags(vk::ImageUsageFlagBits::eColorAttachment);
  if( !(caps.supportedUsageFlags & imageUsage) ) throw std::runtime_error("Surface doesn't support color attachment");

  // caps.maxImageArrayLayers;
  // caps.minImageExtent;
  // caps.currentTransform;
  // caps.supportedTransforms;

  // Choose a pixel format
  auto surfaceFormats = mPhysicalDevices.front().getSurfaceFormatsKHR(mSurface.get());
  // TODO: Just choosing the first one here..
  mSwapChainFormat = surfaceFormats.front().format;
  auto chosenColourSpace = surfaceFormats.front().colorSpace;
  mSwapChainExtent = caps.currentExtent;
  vk::SwapchainCreateInfoKHR info(
        vk::SwapchainCreateFlagsKHR(),
        mSurface.get(),
        numImages, // Num images in the swapchain
        mSwapChainFormat, // Pixel format
        chosenColourSpace, // Colour space
        mSwapChainExtent, // Size of window
        1, // Image array layers
        imageUsage, // Image usage flags
        vk::SharingMode::eExclusive, 0, nullptr, // Exclusive use by one queue
        caps.currentTransform, // flip/rotate before presentation
        alphaMode, // How alpha is mapped
        vk::PresentModeKHR::eFifo, // vsync (mailbox is vsync or faster, fifo is vsync)
        true, // Clipped - For cases where part of window doesn't need to be rendered/is offscreen
        vk::SwapchainKHR() // The old swapchain to delete
        );

  mSwapChain = mDevice->createSwapchainKHRUnique(info);

  mSwapChainImages = mDevice->getSwapchainImagesKHR(mSwapChain.get());

  return mSwapChain.get();
}

void Registrar::createSwapChainImageViews() {
  for( auto i = 0u; i < mSwapChainImages.size(); ++i ) {
    vk::ImageViewCreateInfo info(
    {}, // empty flags
          mSwapChainImages[i],
          vk::ImageViewType::e2D,
          mSwapChainFormat,
    {}, // IDENTITY component mapping
          vk::ImageSubresourceRange(
    {vk::ImageAspectFlagBits::eColor},0, 1, 0, 1)
          );
    mSwapChainImageViews.emplace_back( mDevice->createImageViewUnique(info) );
  }
}

void Registrar::createRenderPass() {
  // The render pass contains stuff like what the framebuffer attachments are and things
  vk::AttachmentDescription colourAttachment = {};
  colourAttachment.setFormat(mSwapChainFormat)
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

  mRenderPass = mDevice->createRenderPassUnique(renderPassInfo);
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
  vk::Viewport viewport(0.f,0.f, mSwapChainExtent.width, mSwapChainExtent.height, 0.f, 1.f);
  vk::Rect2D scissor({0,0},mSwapChainExtent);
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

  mPipelineLayout = mDevice->createPipelineLayoutUnique(layoutInfo);


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


  mGraphicsPipeline = mDevice->createGraphicsPipelineUnique({}, pipelineInfo);

  // Shader modules deleted here, only needed for pipeline init
}

void Registrar::createFramebuffers() {
  for( auto i = 0u; i < mSwapChainImages.size(); ++i ) {
    auto attachment = mSwapChainImageViews[i].get();

    vk::FramebufferCreateInfo info = {};
    info.setRenderPass(mRenderPass.get()) // Compatible with this render pass (don't use it with any other)
        .setAttachmentCount(1) // 1 attachment - The image from the swap chain
        .setPAttachments(&attachment) // Mapped to the first attachment of the render pass
        .setWidth(mSwapChainExtent.width)
        .setHeight(mSwapChainExtent.height)
        .setLayers(1)
        ;

    mSwapChainFrameBuffers.emplace_back( mDevice->createFramebufferUnique(info));
  }
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
  return mDevice->createShaderModuleUnique(info);
}
