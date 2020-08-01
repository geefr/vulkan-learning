/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#include "graphicspipeline.h"
#include "windowintegration.h"
#include "deviceinstance.h"
#include "util.h"

#include <vulkan/vulkan.hpp>

#include <map>

GraphicsPipeline::GraphicsPipeline(WindowIntegration& windowIntegration, DeviceInstance& deviceInstance, bool invertY)
  : Pipeline(deviceInstance)
  , mWindowIntegration(windowIntegration)
  , mInvertY(invertY)
{
  mSamples = windowIntegration.samples();
}

void GraphicsPipeline::createRenderPass() {
  // The render pass contains stuff like what the framebuffer attachments are and things
  auto colourAttachment = vk::AttachmentDescription()
      .setFormat(mWindowIntegration.sampleFormat())
      .setSamples(mSamples)
      .setLoadOp(vk::AttachmentLoadOp::eClear) // What to do before rendering
      .setStoreOp(vk::AttachmentStoreOp::eStore) // What to do after rendering
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined) // Image layout before render pass begins, we don't care, gonna clear anyway
      .setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal) // Image layout to transfer to when render pass finishes, ready for presentation
      ;

  auto depthAttachment = vk::AttachmentDescription()
      .setFormat(mWindowIntegration.depthFormat())
      .setSamples(mSamples)
      .setLoadOp(vk::AttachmentLoadOp::eClear)
      .setStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

  auto colourAttachmentResolve = vk::AttachmentDescription()
      .setFormat(mWindowIntegration.swapChainFormat())
      .setSamples(vk::SampleCountFlagBits::e1)
      .setLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStoreOp(vk::AttachmentStoreOp::eStore)
      .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
      .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
      .setInitialLayout(vk::ImageLayout::eUndefined)
      .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

  // For starters we only need one sub-pass to draw
  // Multiple sub passes are used for multi-pass rendering
  // putting them in one overall render pass means vulkan can reorder
  // them as needed
  auto colourAttachmentRef = vk::AttachmentReference()
      .setAttachment(0) // Index in attachment description array (We only have one so far)
      .setLayout(vk::ImageLayout::eColorAttachmentOptimal); // Layout for the attachment during this subpass. This attachment is our colour buffer so marked as such here
  auto depthAttachmentRef = vk::AttachmentReference()
      .setAttachment(1)
      .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
  auto colourAttachmentResolveRef = vk::AttachmentReference()
      .setAttachment(2)
      .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

  auto subpass = vk::SubpassDescription()
      .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics) // Marked as a graphics subpass, looks like we can mix multiple subpass types in one render pass?
      .setColorAttachmentCount(1)
      .setPColorAttachments(&colourAttachmentRef)
      .setPDepthStencilAttachment(&depthAttachmentRef)
      .setPResolveAttachments(&colourAttachmentResolveRef)
      ;
  // Index of colour attachment here matches layout = 0 in fragment shader
  // So the frag shader can reference multiple attachments as its output!

  /*
   * Setup subpass dependencies
   * This is needed to ensure that the render pass
   * doesn't begin until the image is available
   *
   * Apparently an alternative is to set the waitStages of the command buffer
   * to top of pipe to ensure rendering doesn't begin until the pipeline is started?
   * https://vulkan-tutorial.com/Drawing_a_triangle/Drawing/Rendering_and_presentation
   *
   * TODO: Yeah I don't understand this and the tutorials gloss over it, probably more info in the
   * synchronisation chapter of the book
   */
  auto dep = vk::SubpassDependency()
      .setSrcSubpass(VK_SUBPASS_EXTERNAL) // implicit subpass before this pass
      .setDstSubpass(0) // The index of our subpass
      .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) // Wait on this operation
      .setSrcAccessMask({}) // Which needs to happen in this stage...here we're waiting for the swap chain to finish reading from the image
      .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput) // The operations which should wait are in the colour attachment stage
      .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite) // and involve the writing of the colour attachment
      ;

  // Now let's actually make the render pass
  std::vector<vk::AttachmentDescription> attachments;
  attachments.emplace_back(colourAttachment);
  attachments.emplace_back(depthAttachment);
  attachments.emplace_back(colourAttachmentResolve);
  auto renderPassInfo = vk::RenderPassCreateInfo()
      .setAttachmentCount(attachments.size())
      .setPAttachments(attachments.size() ? attachments.data() : nullptr)
      .setSubpassCount(1)
      .setPSubpasses(&subpass)
      .setDependencyCount(1)
      .setPDependencies(&dep)
      ;

  mRenderPass = mDeviceInstance.device().createRenderPassUnique(renderPassInfo);
  if( !mRenderPass ) throw std::runtime_error("Failed to create render pass");
}

void GraphicsPipeline::createPipeline() {
  /*
   * The programmable parts of the pipeline are similar to gl
   * And in this case the shaders were even compiled from glsl -> SPIR-V
   */

  // Later we just need to hand a pile of shaders to the pipeline
  auto shaderStages = createShaderStageInfo();
  createRenderPass();

  /*
   * Then there's the fixed function sections of the pipeline
   */
  // Vertex input
  // For now nothing here as vertices are hardcoded in the shader
  uint32_t numVertexBindings = static_cast<uint32_t>(mVertexInputBindings.size());
  uint32_t numVertexAttributes = static_cast<uint32_t>(mVertexInputAttributes.size());
  auto vertInputInfo = vk::PipelineVertexInputStateCreateInfo()
      .setVertexBindingDescriptionCount(numVertexBindings)
      .setPVertexBindingDescriptions(numVertexBindings ? mVertexInputBindings.data() : nullptr)
      .setVertexAttributeDescriptionCount(numVertexAttributes)
      .setPVertexAttributeDescriptions(numVertexAttributes ? mVertexInputAttributes.data() : nullptr)
      ;

  // Input assembly
  // Typical triangles setup
  auto inputAssInfo = vk::PipelineInputAssemblyStateCreateInfo()
      .setTopology(mInputAssemblyPrimitiveTopology)
      .setPrimitiveRestartEnable(false)
      ;

  // Viewport
  // TODO: This should be set as dynamic state, passed through in the render pass
  vk::Viewport viewport(0.f,0.f, mWindowIntegration.swapChainExtent().width, mWindowIntegration.swapChainExtent().height, 0.f, 1.f);
  if( mInvertY ) {
      // Flip the viewport
      viewport.height = - static_cast<float>(mWindowIntegration.swapChainExtent().height);
      viewport.y = static_cast<float>(mWindowIntegration.swapChainExtent().height);
  }
  vk::Rect2D scissor({0,0},mWindowIntegration.swapChainExtent());
  auto viewportInfo = vk::PipelineViewportStateCreateInfo()
      .setFlags({})
      .setViewportCount(1)
      .setPViewports(&viewport)
      .setScissorCount(1)
      .setPScissors(&scissor)
      ;

  // Rasteriser
  auto rasterisationInfo = vk::PipelineRasterizationStateCreateInfo()
      .setDepthClampEnable(false)
      .setRasterizerDiscardEnable(false)
      .setPolygonMode(vk::PolygonMode::eFill)
      .setCullMode(vk::CullModeFlagBits::eBack)
      // TODO: This maybe shouldn't be hardcoded here. if mInvertY expect left-handed vertices into shader, else right-handed
      .setFrontFace(mInvertY ? vk::FrontFace::eClockwise : vk::FrontFace::eCounterClockwise)
      .setDepthBiasEnable(false)
      .setDepthBiasConstantFactor(0.f)
      .setDepthClampEnable(false)
      .setDepthBiasClamp(0.f)
      .setLineWidth(1.f)
      ;

  // Multisampling
  auto multisampleInfo = vk::PipelineMultisampleStateCreateInfo()
      .setSampleShadingEnable(false) // Improves sampling of textures at performance cost
      .setMinSampleShading(0.2f)
      .setRasterizationSamples(mSamples);

  // Depth/Stencil test
  auto depthStencil = vk::PipelineDepthStencilStateCreateInfo()
      .setDepthTestEnable(true)
      .setDepthWriteEnable(true)
      .setDepthCompareOp(vk::CompareOp::eLess)
      .setDepthBoundsTestEnable(false)
      .setStencilTestEnable(false);

  // Colour & alpha blending
  // attachment state is per-framebuffer
  // create info is global to the pipeline
  auto colourBlendAttach = vk::PipelineColorBlendAttachmentState()
      .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
      .setBlendEnable(false)
      .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
      .setDstColorBlendFactor(vk::BlendFactor::eOneMinusDstAlpha)
      .setColorBlendOp(vk::BlendOp::eAdd)
      .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
      .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
      .setAlphaBlendOp(vk::BlendOp::eAdd)
      ;

  auto colourBlendInfo = vk::PipelineColorBlendStateCreateInfo()
      .setLogicOpEnable(false)
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
  auto numPushConstantRanges = static_cast<uint32_t>(mPushConstants.size());
  auto numDSLayouts = static_cast<uint32_t>(mDescriptorSetLayouts.size());
  std::vector<vk::DescriptorSetLayout> tmpLayouts;
  for (auto& p : mDescriptorSetLayouts) tmpLayouts.emplace_back(p.get());

  auto layoutInfo = vk::PipelineLayoutCreateInfo()
      .setSetLayoutCount(numDSLayouts)
      .setPSetLayouts(numDSLayouts ? tmpLayouts.data() : nullptr)
      .setPushConstantRangeCount(numPushConstantRanges)
      .setPPushConstantRanges(numPushConstantRanges ? mPushConstants.data() : nullptr)
      ;
  mPipelineLayout = mDeviceInstance.device().createPipelineLayoutUnique(layoutInfo);
  if( !mPipelineLayout ) throw std::runtime_error("Failed to create pipeline layout");

  // Finally let's make the pipeline itself
  auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
      .setStageCount(static_cast<uint32_t>(shaderStages.size()))
      .setPStages(shaderStages.data())
      .setPVertexInputState(&vertInputInfo)
      .setPInputAssemblyState(&inputAssInfo)
      .setPViewportState(&viewportInfo)
      .setPRasterizationState(&rasterisationInfo)
      .setPMultisampleState(&multisampleInfo)
      .setPDepthStencilState(&depthStencil)
      .setPColorBlendState(&colourBlendInfo)
      .setPDynamicState(nullptr)
      .setLayout(mPipelineLayout.get())
      .setRenderPass(mRenderPass.get()) // The render pass the pipeline will be used in
      .setSubpass(0) // The sub pass the pipeline will be used in
      .setBasePipelineHandle({}) // Derive from an existing pipeline
      .setBasePipelineIndex(-1) // Or the index of a pipeline, which may not yet exist. DERIVATIVE_BIT must be specified in flags to do this
      ;


  mPipeline = mDeviceInstance.device().createGraphicsPipelineUnique({}, pipelineInfo);
  if( !mPipeline ) throw std::runtime_error("Failed to create pipeline");
  // Shader modules deleted here, only needed for pipeline init
}


