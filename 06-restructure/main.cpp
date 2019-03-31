#include "util/deviceinstance.h"
#include "util/windowintegration.h"
#include "util/graphicspipeline.h"
#include "util/framebuffer.h"
#include "util/simplebuffer.h"

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#endif

// https://github.com/KhronosGroup/Vulkan-Hpp
# include <vulkan/vulkan.hpp>

#ifdef VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xlib.h>
#endif

#include <iostream>
#include <stdexcept>

class VulkanExperimentApp
{
public:
  VulkanExperimentApp(){}
  ~VulkanExperimentApp(){}

  void run() {
    initWindow();
    initVK();
    loop();
    cleanup();
  }

private:
  // The window itself
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

  // Our classyboys to obfuscate the verbosity of vulkan somewhat
  // Remember deletion order matters
  std::unique_ptr<DeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<GraphicsPipeline> mGraphicsPipeline;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  uint32_t mMaxFramesInFlight = 3u;
  std::vector<vk::UniqueSemaphore> mImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> mRenderFinishedSemaphores;
  std::vector<vk::UniqueFence> mFrameInFlightFences;

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan Experiment", nullptr, nullptr);
  }

  void initVK() {
    // Initialise the vulkan instance
    // GLFW can give us what extensions it requires, nice
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> requiredExtensions;
    for(uint32_t i = 0; i < glfwExtensionCount; ++i ) requiredExtensions.push_back(glfwExtensions[i]);
    mDeviceInstance.reset(new DeviceInstance(requiredExtensions, {}, "vulkan-experiments", 1));

    // Find out what queues are available
    //auto queueFamilyProps = dev.getQueueFamilyProperties();
    //printQueueFamilyProperties(queueFamilyProps);

    // Create a logical device to interact with
    // To do this we also need to specify how many queues from which families we want to create
    // In this case just 1 queue from the first family which supports graphics

    mWindowIntegration.reset(new WindowIntegration(*mDeviceInstance.get(), mWindow));

    mGraphicsPipeline.reset(new GraphicsPipeline(*mWindowIntegration.get(), *mDeviceInstance.get()));
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
     vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
     commandBufferAllocateInfo.setCommandPool(mCommandPool.get())
         .setCommandBufferCount(static_cast<uint32_t>(mWindowIntegration->swapChainImages().size()))
         .setLevel(vk::CommandBufferLevel::ePrimary)
         ;

    mCommandBuffers = mDeviceInstance->device().allocateCommandBuffersUnique(commandBufferAllocateInfo);

    for( auto i = 0u; i < mCommandBuffers.size(); ++i ) {
      auto commandBuffer = mCommandBuffers[i].get();
      vk::CommandBufferBeginInfo beginInfo = {};
      beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution - TODO: Wat?
          .setPInheritanceInfo(nullptr)
          ;
      commandBuffer.begin(beginInfo);

      // Start the render pass
      vk::RenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.setRenderPass(mGraphicsPipeline->renderPass())
                    .setFramebuffer(mFrameBuffer->frameBuffers()[i].get());
      renderPassInfo.renderArea.offset = vk::Offset2D(0,0);
      renderPassInfo.renderArea.extent = mWindowIntegration->extent();
      // Info for attachment load op clear
      std::array<float,4> col = {0.f,.0f,0.f,1.f};
      vk::ClearValue clearColour(col);
      renderPassInfo.setClearValueCount(1)
        .setPClearValues(&clearColour)
        ;
      // render commands will be embedded in primary buffer and no secondary command buffers
      // will be executed
      commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

      // Now it's time to finally draw our one crappy little triangle >.<
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline->graphicsPipeline().get());
      commandBuffer.draw(3, // Draw 3 vertices
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

  void loop() {
    auto frameIndex = 0u;

    while(!glfwWindowShouldClose(mWindow)) {
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

  void cleanup() {
    // Explicitly cleanup the vulkan objects here
    // Ensure they are shut down before terminating glfw
    // TODO: Could wrap the glfw stuff in a smart pointer and
    // remove the need for this method

    mDeviceInstance->waitAllDevicesIdle();

    for(auto& p : mFrameInFlightFences) p.reset();
    for(auto& p : mRenderFinishedSemaphores) p.reset();
    for(auto& p : mImageAvailableSemaphores) p.reset();
    for(auto& p : mCommandBuffers) p.reset();
    mCommandPool.reset();
    mGraphicsPipeline.reset();
    mFrameBuffer.reset();
    mWindowIntegration.reset();
    mDeviceInstance.reset();

    glfwDestroyWindow(mWindow);
    glfwTerminate();
  }
};

/// Populate a command buffer in order to copy data from source to dest
void fillCB_CopyBufferObjects( vk::CommandBuffer& commandBuffer, vk::Buffer& src, vk::Buffer& dst, vk::DeviceSize srcOff, vk::DeviceSize dstOff, vk::DeviceSize size, std::string testData )
{
  // Start the buffer
  // Default flags
  // - We might execute this buffer more than once
  // - We won't execute it multiple times simultaneously
  // - We're not creating a secondary command buffer
  vk::CommandBufferBeginInfo beginInfo;
  commandBuffer.begin(&beginInfo);

  // TODO: There's some syntax error here
  //vk::ArrayProxy<vk::BufferCopy> cpy = {{srcOff, dstOff, size}};
  //commandBuffer.copyBuffer(src, dst, cpy.size(), cpy.data());

  // TODO: size here must be a multiple of 4
  // Buffer must be created with TRANSFER_DST_BIT set
  commandBuffer.updateBuffer(src, 0, size, testData.data());

  vk::BufferCopy cpy(srcOff, dstOff, size);
  commandBuffer.copyBuffer(src, dst, 1, &cpy);

  // End the buffer
  commandBuffer.end();
}

int main(int argc, char* argv[])
{
  try {
    VulkanExperimentApp app;
    app.run();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
