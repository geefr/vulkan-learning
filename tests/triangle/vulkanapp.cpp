#include "vulkanapp.h"
#include <mutex>

void VulkanApp::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan Experiment", nullptr, nullptr);
}

void VulkanApp::initVK() {
  // Initialise the vulkan instance
  // GLFW can give us what extensions it requires, nice
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char*> requiredExtensions;
  for(uint32_t i = 0; i < glfwExtensionCount; ++i ) requiredExtensions.push_back(glfwExtensions[i]);
  mDeviceInstance.reset(new DeviceInstance(requiredExtensions, {}, "Vulkan Test Application", 1));

  // Find out what queues are available
  //auto queueFamilyProps = dev.getQueueFamilyProperties();
  //printQueueFamilyProperties(queueFamilyProps);

  // Create a logical device to interact with
  // To do this we also need to specify how many queues from which families we want to create
  // In this case just 1 queue from the first family which supports graphics

  mWindowIntegration.reset(new WindowIntegration(*mDeviceInstance.get(), mWindow));

  mGraphicsPipeline.reset(new GraphicsPipeline(*mWindowIntegration.get(), *mDeviceInstance.get()));

  // Build the graphics pipeline
  // In this case we can throw away the shader modules after building as they're only used by the one pipeline
  {
    auto vertShader = mGraphicsPipeline->createShaderModule("vert.spv");
    auto fragShader = mGraphicsPipeline->createShaderModule("frag.spv");
    mGraphicsPipeline->shaders().mVertexShader = vertShader.get();
    mGraphicsPipeline->shaders().mFragmentShader = fragShader.get();

    // The layout of our vertex buffers
    auto vertBufferBinding = vk::VertexInputBindingDescription()
        .setBinding(0)
        .setStride(sizeof(VertexData))
        .setInputRate(vk::VertexInputRate::eVertex);
    mGraphicsPipeline->vertexInputBindings().emplace_back(vertBufferBinding);

    // Location, Binding, Format, Offset
    mGraphicsPipeline->vertexInputAttributes().emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexData, vertCoord));
    mGraphicsPipeline->vertexInputAttributes().emplace_back(1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(VertexData, vertColour));

    // Register our push constant block
    mPushConstant_test_range = vk::PushConstantRange()
        .setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setOffset(0)
        .setSize(sizeof(PushConstant_test));
    mGraphicsPipeline->pushConstants().emplace_back(mPushConstant_test_range);

    // Create and upload vertex buffers
    createVertexBuffers();

    mGraphicsPipeline->build();
  }

  mFrameBuffer.reset(new FrameBuffer(mDeviceInstance->device(), *mWindowIntegration.get(), mGraphicsPipeline->renderPass()));

  // Setup our sync primitives
  // imageAvailable - gpu: Used to stall the pipeline until the presentation has finished reading from the image
  // renderFinished - gpu: Used to stall presentation until the pipeline is finished
  // frameInFlightFence - cpu: Used to ensure we don't schedule a second frame for each image until the last is complete

  // Create the semaphores we're gonna use
  mMaxFramesInFlight = mWindowIntegration->swapChainImages().size();
  for( auto i = 0u; i < mMaxFramesInFlight; ++i ) {
    mImageAvailableSemaphores.emplace_back( mDeviceInstance->device().createSemaphoreUnique({}));
    mRenderFinishedSemaphores.emplace_back( mDeviceInstance->device().createSemaphoreUnique({}));
    // Create fence in signalled state so first wait immediately returns and resets fence
    mFrameInFlightFences.emplace_back( mDeviceInstance->device().createFenceUnique({vk::FenceCreateFlagBits::eSignaled}));
  }

  // TODO: Misfit
  mCommandPool = mDeviceInstance->createCommandPool( { /* vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
                                                                                                      vk::CommandPoolCreateFlagBits::eResetCommandBuffer // Buffers can be reset individually, instead of needing to reset the entire pool
                                                                                                                                              */
                                                     });

  // Now make a command buffer for each framebuffer
  auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
      .setCommandPool(mCommandPool.get())
      .setCommandBufferCount(static_cast<uint32_t>(mWindowIntegration->swapChainImages().size()))
      .setLevel(vk::CommandBufferLevel::ePrimary)
      ;

  mCommandBuffers = mDeviceInstance->device().allocateCommandBuffersUnique(commandBufferAllocateInfo);

  for( auto i = 0u; i < mCommandBuffers.size(); ++i ) {
    auto commandBuffer = mCommandBuffers[i].get();
    auto beginInfo = vk::CommandBufferBeginInfo()
        .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution - TODO: Wat?
        .setPInheritanceInfo(nullptr)
        ;
    commandBuffer.begin(beginInfo);

    // Start the render pass
    // Info for attachment load op clear
    std::array<float,4> col = {0.f,.0f,0.f,1.f};
    vk::ClearValue clearColour(col);

    auto renderPassInfo = vk::RenderPassBeginInfo()
        .setRenderPass(mGraphicsPipeline->renderPass())
        .setFramebuffer(mFrameBuffer->frameBuffers()[i].get())
        .setClearValueCount(1)
        .setPClearValues(&clearColour);
    renderPassInfo.renderArea.offset = vk::Offset2D(0,0);
    renderPassInfo.renderArea.extent = mWindowIntegration->extent();

    // render commands will be embedded in primary buffer and no secondary command buffers
    // will be executed
    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    // Now it's time to finally draw our one crappy little triangle >.<
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline->pipeline());

    // Set our push constants
    PushConstant_test pushConstants = {1.5};
    commandBuffer.pushConstants(
          mGraphicsPipeline->pipelineLayout(),
          mPushConstant_test_range.stageFlags,
          mPushConstant_test_range.offset,
          sizeof(PushConstant_test),
          &pushConstants);


    auto& vertBuf = mVertexBuffers["triangle"];
    //auto& vertBuf = mVertexBuffers["rectangle"];
    vk::Buffer buffers[] = { vertBuf->buffer() };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);
    commandBuffer.draw(static_cast<uint32_t>(vertBuf->size() / sizeof(VertexData)), // Draw n vertices
                       1, // Used for instanced rendering, 1 otherwise
                       0, // First vertex
                       0  // First instance
                       );

    // End the render pass
    commandBuffer.endRenderPass();

    // End the command buffer
    commandBuffer.end();
  }
}

void VulkanApp::createVertexBuffers() {
  // A pretty triangle
  {
    VertexData vertData[] = {
      {{0,-.5,0},{1,0,0,1}},
      {{-.5,.5,0},{0,1,0,1}},
      {{.5,.5,0},{0,0,1,1}},
    };
    mVertexBuffers["triangle"].reset( new SimpleBuffer(
                                        *mDeviceInstance.get(),
                                        sizeof(vertData),
                                        vk::BufferUsageFlagBits::eVertexBuffer) );
    auto& vertBuffer = mVertexBuffers["triangle"];

    std::memcpy(vertBuffer->map(), vertData, sizeof(vertData));
    vertBuffer->flush();
    vertBuffer->unmap();
  }

  // A pretty quad
  {
    VertexData vertData[] = {
      {{-.5,-.5,0},{1,0,0,1}},
      {{-.5,.5,0},{0,1,0,1}},
      {{ .5,-.5,0},{0,0,1,1}},

      {{ .5,-.5,0},{0,0,1,1}},
      {{-.5,.5,0},{0,1,0,1}},
      {{.5,.5,0},{1,0,1,.5}},
    };
    mVertexBuffers["rectangle"].reset( new SimpleBuffer(
                                        *mDeviceInstance.get(),
                                        sizeof(vertData),
                                        vk::BufferUsageFlagBits::eVertexBuffer) );
    auto& vertBuffer = mVertexBuffers["rectangle"];

    std::memcpy(vertBuffer->map(), vertData, sizeof(vertData));
    vertBuffer->flush();
    vertBuffer->unmap();
  }
}

void VulkanApp::loop() {
  auto frameIndex = 0u;


  while(!glfwWindowShouldClose(mWindow)) {
    std::once_flag windowShown;
    std::call_once(windowShown, [&win=mWindow](){glfwShowWindow(win);});

    glfwPollEvents();

    // Wait for the last frame to finish rendering
    mDeviceInstance->device().waitForFences(1, &mFrameInFlightFences[frameIndex].get(), true, std::numeric_limits<uint64_t>::max());

    // Advance to next frame index, loop at max
    frameIndex++;
    if( frameIndex == mMaxFramesInFlight ) frameIndex = 0;

    // Reset the fence - fences must be reset before being submitted
    auto frameFence = mFrameInFlightFences[frameIndex].get();
    mDeviceInstance->device().resetFences(1, &frameFence);

    // Acquire and image from the swap chain
    auto imageIndex = mDeviceInstance->device().acquireNextImageKHR(
          mWindowIntegration->swapChain(), // Get an image from this
          std::numeric_limits<uint64_t>::max(), // Don't timeout
          mImageAvailableSemaphores[frameIndex].get(), // semaphore to signal once presentation is finished with the image
          vk::Fence()).value; // Dummy fence, we don't care here

    // Submit the command buffer
    vk::SubmitInfo submitInfo = {};
    auto commandBuffer = mCommandBuffers[imageIndex].get();

    // Don't execute until this is ready
    vk::Semaphore waitSemaphores[] = {mImageAvailableSemaphores[frameIndex].get()};
    // place the wait before writing to the colour attachment

    // If we have render pass depedencies
    vk::PipelineStageFlags waitStages[] {vk::PipelineStageFlagBits::eColorAttachmentOutput};

    // Otherwise wait until the pipeline starts to allow reading?
    //vk::PipelineStageFlags waitStages[] {vk::PipelineStageFlagBits::eTopOfPipe};


    // Signal this semaphore when rendering is done
    vk::Semaphore signalSemaphores[] = {mRenderFinishedSemaphores[frameIndex].get()};
    submitInfo.setWaitSemaphoreCount(1)
        .setPWaitSemaphores(waitSemaphores)
        .setPWaitDstStageMask(waitStages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&commandBuffer)
        .setSignalSemaphoreCount(1)
        .setPSignalSemaphores(signalSemaphores)
        ;

    vk::ArrayProxy<vk::SubmitInfo> submits(submitInfo);
    // submit, signal the frame fence at the end
    mDeviceInstance->queue().submit(submits.size(), submits.data(), frameFence);

    // Present the results of a frame to the swap chain
    vk::SwapchainKHR swapChains[] = {mWindowIntegration->swapChain()};
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.setWaitSemaphoreCount(1)
        .setPWaitSemaphores(signalSemaphores) // Wait before presentation can start
        .setSwapchainCount(1)
        .setPSwapchains(swapChains)
        .setPImageIndices(&imageIndex)
        .setPResults(nullptr)
        ;

    // TODO: Force-using a single queue for both graphics and present
    // Some systems may not be able to support this
    mDeviceInstance->queue().presentKHR(presentInfo);
  }
}

void VulkanApp::cleanup() {
  // Explicitly cleanup the vulkan objects here
  // Ensure they are shut down before terminating glfw
  // TODO: Could wrap the glfw stuff in a smart pointer and
  // remove the need for this method

  mDeviceInstance->waitAllDevicesIdle();

  //for(auto& p : mFrameInFlightFences) p.reset();
  //for(auto& p : mRenderFinishedSemaphores) p.reset();
  //for(auto& p : mImageAvailableSemaphores) p.reset();
  //for(auto& p : mCommandBuffers) p.reset();
  mFrameInFlightFences.clear();
  mRenderFinishedSemaphores.clear();
  mImageAvailableSemaphores.clear();
  mCommandBuffers.clear();
  mCommandPool.reset();
  mGraphicsPipeline.reset();
  mFrameBuffer.reset();
  mWindowIntegration.reset();

  mVertexBuffers.clear();

  mDeviceInstance.reset();

  glfwDestroyWindow(mWindow);
  glfwTerminate();
}
