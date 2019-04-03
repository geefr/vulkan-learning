#include "vulkanapp.h"
#include <mutex>
#include <chrono>

#include "glm/gtc/matrix_transform.hpp"

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
    mGraphicsPipeline->inputAssembly_primitiveTopology(vk::PrimitiveTopology::ePointList);

    // The layout of our vertex buffers
    auto vertBufferBinding = vk::VertexInputBindingDescription()
        .setBinding(0)
        .setStride(sizeof(Physics::Particle))
        .setInputRate(vk::VertexInputRate::eVertex);
    mGraphicsPipeline->vertexInputBindings().emplace_back(vertBufferBinding);

    // Location, Binding, Format, Offset
    // TODO: If we're really only care about position/dimensions here uploading the whole buffer to the gpu is kinda wasteful
    mGraphicsPipeline->vertexInputAttributes().emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Physics::Particle, position));
    mGraphicsPipeline->vertexInputAttributes().emplace_back(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Physics::Particle, colour));

    // Register our push constant block
    mPushContantsRange = vk::PushConstantRange()
        .setStageFlags(vk::ShaderStageFlagBits::eVertex)
        .setOffset(0)
        .setSize(sizeof(PushConstants));
    mGraphicsPipeline->pushConstants().emplace_back(mPushContantsRange);

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
  mCommandPool = mDeviceInstance->createCommandPool( { vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                                                       /* vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
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
    buildCommandBuffer(commandBuffer, mFrameBuffer->frameBuffers()[i].get(), mParticleVertexBuffers[i]->buffer());
  }
}

void VulkanApp::buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer, vk::Buffer& particleVertexBuffer) {

  auto beginInfo = vk::CommandBufferBeginInfo()
      .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution
      .setPInheritanceInfo(nullptr)
      ;
  commandBuffer.begin(beginInfo);

  // Start the render pass
  // Info for attachment load op clear
  std::array<float,4> col = {0.f,.0f,0.f,1.f};
  vk::ClearValue clearColour(col);

  auto renderPassInfo = vk::RenderPassBeginInfo()
      .setRenderPass(mGraphicsPipeline->renderPass())
      .setFramebuffer(frameBuffer)
      .setClearValueCount(1)
      .setPClearValues(&clearColour);
  renderPassInfo.renderArea.offset = vk::Offset2D(0,0);
  renderPassInfo.renderArea.extent = mWindowIntegration->extent();

  auto particleBufSize = static_cast<uint32_t>(mPhysics.particles().size() * sizeof(Physics::Particle));

  // TODO: Remove the need to push the entire buffer here
  // Mapping the buffer may be better, but we'd at least want a set of buffers to rotate through and would need to deal with synchronisation
  // Better would be to push the whole physics simulation onto the gpu (the real intention of this sample) and then only need to
  // push information about external forces and such in
  //
  // We're also limited to 65Kb here, which is an entirely sane and reasonable restriction to place on such a function
  //
  // This however does work, and is a super convenient way to get data in
  // This would be the way to update uniform buffers and such if not using the push constant block

  // As we're uploading a huge amount of data this isn't exactly sensible
  // Upload in chunks to avoid the 65K limit for now, I'll improve this later as this is clearly the limiting factor for performance
  auto chunkSize = 1000u;
  for( auto i = 0u; i < mPhysics.particles().size(); i+=chunkSize ) {
    auto numToUpload = chunkSize;
    if( i + numToUpload >= mPhysics.particles().size() ) {
      numToUpload = static_cast<uint32_t>(mPhysics.particles().size() - i);
    }

    commandBuffer.updateBuffer(particleVertexBuffer, i * sizeof(Physics::Particle), numToUpload * sizeof(Physics::Particle), mPhysics.particles().data() + i);
  }

  // While this is a nice way to update buffer contents we do need to synchronise, this is counted as a 'transfer operation' for synchronisation
  // This may seem to work without the barrier here, as we've got 1 copy of the buffer per fif, and have already ensured the previous execution
  // of this command buffer has finished with the fence
  //
  // But we do need to synchronise the write to the buffer against the read within the pipeline, it's possible the gpu won't have flushed the caches
  // fully unless we tell it what this buffer needs to be ready for

  // Barrier to prevent the start of vertex shader until writing has finished to vertex attributes
  // Remember:
  // - Access flags should be as minimal as possible here
  // - Barriers must be outside a render pass (there's an exception to this, but keep it simple for now)
  auto particleBufferBarrier = vk::BufferMemoryBarrier()
      .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
      .setDstAccessMask(vk::AccessFlagBits::eVertexAttributeRead)
      .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
      .setBuffer(particleVertexBuffer)
      .setOffset(0)
      .setSize(VK_WHOLE_SIZE)
      ;
  commandBuffer.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlagBits::eByRegion,
        0, nullptr,
        1, &particleBufferBarrier,
        0, nullptr
        );

  // render commands will be embedded in primary buffer and no secondary command buffers
  // will be executed
  commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline->pipeline());

  // Set our push constants
  commandBuffer.pushConstants(
        mGraphicsPipeline->pipelineLayout(),
        mPushContantsRange.stageFlags,
        mPushContantsRange.offset,
        sizeof(PushConstants),
        &mPushConstants);

  vk::Buffer buffers[] = { particleVertexBuffer };
  vk::DeviceSize offsets[] = { 0 };
  commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);

  commandBuffer.draw(mPhysics.particles().size(), // Draw n vertices
                     1, // Used for instanced rendering, 1 otherwise
                     0, // First vertex
                     0  // First instance
                     );

  // End the render pass
  commandBuffer.endRenderPass();

  // End the command buffer
  commandBuffer.end();
}

void VulkanApp::createVertexBuffers() {
  // Our particles
  // TODO: This is device local to avoid allocating a large host-accessible buffer
  // Apparently >500 particles makes the driver hang like it's constantly swapping data in/out
  auto bufSize = sizeof(Physics::Particle) * mPhysics.particles().size();
  mParticleVertexBuffers.reserve(mWindowIntegration->swapChainImages().size());
  for( auto i = 0u; i < mWindowIntegration->swapChainImages().size(); ++i ) {
    mParticleVertexBuffers.emplace_back( new SimpleBuffer(
                                         *mDeviceInstance.get(),
                                         bufSize,
                                         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                         vk::MemoryPropertyFlagBits::eDeviceLocal) );
  }


  // Obviously we can't memmap device local memory
  //std::memcpy(mParticleVertexBuffer->map(), mPhysics.particles().data(), bufSize);
  //mParticleVertexBuffer->unmap();
}

void VulkanApp::loop() {
  auto frameIndex = 0u;

  std::once_flag windowShown;
  std::call_once(windowShown, [&win=mWindow](){glfwShowWindow(win);});

  mLastTime = now();
  mCurTime = mLastTime;

  auto maxIter = 100u;
  auto iter = 0u;


  glm::vec3 eyePos = { 0,90,90 };
  float modelRot = 0.f;

  while(!glfwWindowShouldClose(mWindow) ) {//&& iter++ < maxIter) {

    glfwPollEvents();
/*
    mPushConstants.scale += mPushConstantsScaleFactorDelta;
    if( mPushConstants.scale > 2.0f ) mPushConstantsScaleFactorDelta = -.025f;
    if( mPushConstants.scale < 0.25f ) {
      mPushConstantsScaleFactorDelta = .025f;
      scaleCount++;
    }
    if( scaleCount == 1 ) {
      scaleCount = 0;
      if( mGeomName == "triangle" ) mGeomName = "rectangle";
      else if( mGeomName == "rectangle" ) mGeomName = "triangle";
    }*/

    // Wait for the last frame to finish rendering
    mDeviceInstance->device().waitForFences(1, &mFrameInFlightFences[frameIndex].get(), true, std::numeric_limits<uint64_t>::max());


    // Physics hacks
    mLastTime = mCurTime;
    mCurTime = now();

    mPhysics.step(static_cast<float>(mCurTime - mLastTime));
    //mPhysics.logParticles();

    // Setup matrices
    // Remember now vulkan is z[0,1] +y=down, gl is z[-1,1] +y=up
    // glm should work with the defines we've set
    // So now we've got world space as right handed (y up) but everything after that is left handed (y down)
    //if( eyePos.y < 20 ) eyePos.y += 0.05;

    modelRot += .1f;
    mPushConstants.modelMatrix = glm::mat4(1);//glm::rotate(glm::mat4(1), glm::radians(modelRot), glm::vec3(0.f,-1.f,0.f));
    mPushConstants.viewMatrix = glm::lookAt( eyePos, glm::vec3(0,-100,0), glm::vec3(0,-1,0));
    mPushConstants.projMatrix = glm::perspective(glm::radians(90.f),static_cast<float>(mWindowWidth / mWindowHeight), 0.001f,1000.f);

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
    auto frameBuffer = mFrameBuffer->frameBuffers()[imageIndex].get();

    // Rebuild the command buffer every frame
    // This isn't the most efficient but we're at least re-using the command buffer
    // In a most complex application we would have multiple command buffers and only rebuild
    // the section that needs changing..I think
    buildCommandBuffer(commandBuffer, frameBuffer, mParticleVertexBuffers[imageIndex]->buffer());

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

  mFrameInFlightFences.clear();
  mRenderFinishedSemaphores.clear();
  mImageAvailableSemaphores.clear();
  mCommandBuffers.clear();
  mCommandPool.reset();
  mGraphicsPipeline.reset();
  mFrameBuffer.reset();
  mWindowIntegration.reset();

  mParticleVertexBuffers.clear();

  mDeviceInstance.reset();

  glfwDestroyWindow(mWindow);
  glfwTerminate();
}

double VulkanApp::now() {
  auto duration = std::chrono::system_clock::now().time_since_epoch();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  return millis / 1000.0;
}
