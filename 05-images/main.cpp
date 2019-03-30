
#include "util/registrar.h"
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
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

  vk::Instance mInstance;
  vk::PhysicalDevice mPDevice;
  vk::Device mDevice;
  vk::Queue mGraphicsQueue;
  vk::Queue mPresentQueue;
  vk::CommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  vk::UniqueSemaphore mImageAvailableSemaphore;
  vk::UniqueSemaphore mRenderFinishedSemaphore;

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan Experiment", nullptr, nullptr);
  }

  void initVK() {
    auto& reg = Registrar::singleton();

    // Initialise the vulkan instance
    {
      // GLFW can give us what extensions it requires, nice
      uint32_t glfwExtensionCount = 0;
      const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
      std::vector<const char*> requiredExtensions;
      for(uint32_t i = 0; i < glfwExtensionCount; ++i ) requiredExtensions.push_back(glfwExtensions[i]);
      mInstance = reg.createVulkanInstance(requiredExtensions, "vulkan-experiments", 1);
    }

    mPDevice = reg.physicalDevice();

    // Find out what queues are available
    //auto queueFamilyProps = dev.getQueueFamilyProperties();
    //reg.printQueueFamilyProperties(queueFamilyProps);

    // Create a logical device to interact with
    // To do this we also need to specify how many queues from which families we want to create
    // In this case just 1 queue from the first family which supports graphics
#ifdef VK_USE_PLATFORM_XLIB_KHR
    auto X11dpy = XOpenDisplay(nullptr);
    auto X11vis = DefaultVisual(X11dpy, 0);
    auto X11screen = DefaultScreen(X11dpy);
    auto X11depth = DefaultDepth(X11dpy, 0);
    XSetWindowAttributes X11WindowAttribs;
    X11WindowAttribs.background_pixel = XWhitePixel(X11dpy, 0);

    auto X11Window = XCreateWindow(
          X11dpy,
          XRootWindow(X11dpy, 0),
          0, 0, 800, 600, 0, X11depth,
          InputOutput, X11vis, CWBackPixel,
          &X11WindowAttribs );
    XStoreName(X11dpy, X11Window, "Vulkan Experiment");
    XSelectInput(X11dpy, X11Window, ExposureMask | StructureNotifyMask | KeyPressMask);
    XMapWindow(X11dpy, X11Window);

    auto logicalDevice = reg.createLogicalDeviceWithPresentQueueXlib(vk::QueueFlagBits::eGraphics, X11dpy, XVisualIDFromVisual(X11vis));
    auto surface = reg.createSurfaceXlib(X11dpy, X11Window);
    auto swapChain = reg.createSwapChainXlib();
#elif defined(USE_GLFW)
    mDevice = reg.createLogicalDevice(vk::QueueFlagBits::eGraphics);
    reg.createSurfaceGLFW(mWindow);
#else
    mDevice = reg.createLogicalDevice(vk::QueueFlagBits::eGraphics);
#endif

    // Get the queue from the device
    // queues are the thing that actually do the work
    // could be a subprocessor on the device, or some other subsection of capability
    mGraphicsQueue = reg.queue();
    mPresentQueue = reg.queue(); // Intentionally the same at first, shouldn't be doing it like this really as there's a chance we don't have 1 queue that supports both graphics and present
    reg.createSwapChain();
    reg.swapChainImages();
    reg.createSwapChainImageViews();
    reg.createRenderPass();
    reg.createGraphicsPipeline();
    reg.createFramebuffers();

    // Create the semaphores we're gonna use
    mImageAvailableSemaphore = mDevice.createSemaphoreUnique({});
    mRenderFinishedSemaphore = mDevice.createSemaphoreUnique({});

    // TODO: Misfit
    mCommandPool = reg.createCommandPool( { /* vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
                                                 vk::CommandPoolCreateFlagBits::eResetCommandBuffer // Buffers can be reset individually, instead of needing to reset the entire pool
                                                                                         */
                                              });

    // Now make a command buffer for each framebuffer
     vk::CommandBufferAllocateInfo commandBufferAllocateInfo = {};
     commandBufferAllocateInfo.setCommandPool(mCommandPool)
         .setCommandBufferCount(static_cast<uint32_t>(reg.swapChainImages().size()))
         .setLevel(vk::CommandBufferLevel::ePrimary)
         ;

    mCommandBuffers = reg.device().allocateCommandBuffersUnique(commandBufferAllocateInfo);

    for( auto i = 0u; i < mCommandBuffers.size(); ++i ) {
      auto commandBuffer = mCommandBuffers[i].get();
      vk::CommandBufferBeginInfo beginInfo = {};
      beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution - TODO: Wat?
          .setPInheritanceInfo(nullptr)
          ;
      commandBuffer.begin(beginInfo);

      // Start the render pass
      vk::RenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.setRenderPass(reg.renderPass())
                    .setFramebuffer(reg.frameBuffers()[i].get());
      renderPassInfo.renderArea.offset = vk::Offset2D(0,0);
      renderPassInfo.renderArea.extent = reg.swapChainExtent();
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
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, reg.graphicsPipeline().get());
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
    auto& reg = Registrar::singleton();

    while(!glfwWindowShouldClose(mWindow)) {
      glfwPollEvents();

      // Acquire and image from the swap chain
      auto imageIndex = mDevice.acquireNextImageKHR(
            reg.swapChain(), // Get an image from this
            std::numeric_limits<uint64_t>::max(), // Don't timeout
            mImageAvailableSemaphore.get(), // semaphore to signal once presentation is finished with the image
            vk::Fence()).value; // Dummy fence, we don't care here

      // Submit the command buffer
      vk::SubmitInfo submitInfo = {};
      auto commandBuffer = mCommandBuffers[imageIndex].get();
      // Don't execute until this is ready
      vk::Semaphore waitSemaphores[] = {mImageAvailableSemaphore.get()};
      // place the wait before writing to the colour attachment

      // If we have render pass depedencies
      vk::PipelineStageFlags waitStages[] {vk::PipelineStageFlagBits::eColorAttachmentOutput};

      // Otherwise wait until the pipeline starts to allow reading?
      //vk::PipelineStageFlags waitStages[] {vk::PipelineStageFlagBits::eTopOfPipe};


      // Signal this semaphore when rendering is done
      vk::Semaphore signalSemaphores[] = {mRenderFinishedSemaphore.get()};
      submitInfo.setWaitSemaphoreCount(1)
          .setPWaitSemaphores(waitSemaphores)
          .setPWaitDstStageMask(waitStages)
          .setCommandBufferCount(1)
          .setPCommandBuffers(&commandBuffer)
          .setSignalSemaphoreCount(1)
          .setPSignalSemaphores(signalSemaphores)
          ;

      vk::ArrayProxy<vk::SubmitInfo> submits(submitInfo);
      mGraphicsQueue.submit(submits.size(), submits.data(), vk::Fence());

      // Present the results of a frame to the swap chain
      vk::SwapchainKHR swapChains[] = {reg.swapChain()};
      vk::PresentInfoKHR presentInfo = {};
      presentInfo.setWaitSemaphoreCount(1)
          .setPWaitSemaphores(signalSemaphores) // Wait before presentation can start
          .setSwapchainCount(1)
          .setPSwapchains(swapChains)
          .setPImageIndices(&imageIndex)
          .setPResults(nullptr)
          ;

      mPresentQueue.presentKHR(presentInfo);

      // Wait until everything is finished (Not efficient, but avoids filling up the queue into infinity :P)
      mPresentQueue.waitIdle();
    }
  }

  void cleanup() {
    glfwDestroyWindow(mWindow);
    Registrar::singleton().tearDown();
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
