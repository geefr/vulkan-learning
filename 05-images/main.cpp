
#include "util/registrar.h"
#include "util/simplebuffer.h"

// https://github.com/KhronosGroup/Vulkan-Hpp
#include <vulkan/vulkan.hpp>

#ifdef VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xlib.h>
#endif

#include <iostream>
#include <stdexcept>

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
    auto& reg = Registrar::singleton();
    reg.createVulkanInstance("vulken-experiments", 1);

    // Great we found some devices, let's see what the hardware can do
    std::cout << "Found " << reg.physicalDevices().size() << " physical devices (First will be used from here on)" << std::endl;
    for( auto& p : reg.physicalDevices() ) reg.printPhysicalDeviceProperties(p);

    // Let's print some further info about a device
    // The first device should now be the 'best'
    auto dev = reg.physicalDevice();
    //reg.printDetailedPhysicalDeviceInfo(dev);

    // Find out what queues are available
    auto queueFamilyProps = dev.getQueueFamilyProperties();
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
 //   auto logicalDevice = reg.createLogicalDevice(vk::QueueFlagBits::eGraphics);
#else
# error "Code path not implemented"
#endif

    std::cout << "Created a logical device: " << std::hex << reinterpret_cast<uint64_t>(&(logicalDevice)) << std::endl;

    // Get the queue from the device
    // queues are the thing that actually do the work
    // could be a subprocessor on the device, or some other subsection of capability
    auto q = reg.queue();

    // A pool for allocating command buffers
    // Flags here are flexible, but may add a small overhead
    auto commandPool = reg.createCommandPool( {  vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
                                                 vk::CommandPoolCreateFlagBits::eResetCommandBuffer // Buffers can be reset individually, instead of needing to reset the entire pool
                                              });

    // Now make a command buffer
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
          commandPool, // The pool to allocate from
          vk::CommandBufferLevel::ePrimary, // A primary buffer (Secondary buffers are ones that can be called by other command buffers)
          1 // Just the one for now
          );

    auto commandBuffers = logicalDevice.allocateCommandBuffersUnique(commandBufferAllocateInfo);
    if( commandBuffers.empty() ) throw std::runtime_error("Failed to allocate command buffer");
    // I guess normally you'd allocate a bunch, but only 1 here now
    // pull it out of the vector
    auto commandBuffer = std::move(commandBuffers.front()); commandBuffers.clear();

    // This is pointless in this trivial example, but here's how to reset all buffers in a pool.
    // Recycling command buffers should be more efficient than recreating all the time
    // Typically this is used at the end of a frame to return the buffers back to the pool for next time
    // If reset without returning resources to the pool buffer complexity should remain constant
    // to avoid unecessary bloat if a buffer becomes smaller than the previous usage
    // Alternatively when resetting specify the resource reset bit (but assuming not every frame?)
    logicalDevice.resetCommandPool(commandPool, vk::CommandPoolResetFlagBits::eReleaseResources);


    // Setup 2 buffers to play with
    // 1: Source buffer containing some simple test data
    // 2: Empty buffer to copy this data into
    // TODO: Length here hardcoded to multiple of 4 to ensure alignment, deal with this later :P
    std::string testData("Hello, World! Live Long and prosper \\o/ ");
    auto bufSize = testData.size() * sizeof(std::string::value_type);

    // Scoping braces to force buffers to be deleted before registrar teardown
    {
      SimpleBuffer srcBuffer(
            bufSize,
            {vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst},
            {vk::MemoryPropertyFlagBits::eHostVisible}
            );
      SimpleBuffer dstBuffer(
            bufSize,
            {vk::BufferUsageFlagBits::eTransferDst},
            {vk::MemoryPropertyFlagBits::eHostVisible}
            );
/*
      auto pSrcBuffer = srcBuffer.map();
      std::memcpy(pSrcBuffer, testData.data(), bufSize);
      srcBuffer.flush();
      srcBuffer.unmap();
      std::cout << "Successfully setup buffers and mapped test data into source buffer" << std::endl;
*/
      // Fill the command buffer with our commands
      fillCB_CopyBufferObjects(commandBuffer.get(), srcBuffer.buffer(), dstBuffer.buffer(), 0, 0, bufSize, testData);

      // And submit the buffer to the queue to start doing stuff
      vk::SubmitInfo submitInfo(
            0,nullptr,
            nullptr,
            1,
            &commandBuffer.get(),
            0,
            nullptr
            );
      vk::Fence fence;
      q.submit(1, &submitInfo, fence);

      // Wait for everything to finish before cleanup
      // Cleanup handled by the smart handles in this case
      logicalDevice.waitIdle();

      // Now read back from the destination buffer
      std::string dstString(bufSize, '\0');
      auto pDstBuffer = dstBuffer.map();
      dstBuffer.flush();
      std::memcpy(&dstString[0], pDstBuffer, bufSize);
      dstBuffer.unmap();

      std::cout << "Read string from dstBuffer: " << dstString << std::endl;
      if( testData != dstString ) throw std::runtime_error("ERORR: Failed to copy data from source -> dest buffer");
    }

    while (true) {
        XEvent ev;
        XNextEvent(X11dpy, &ev);
        if (ev.type == Expose) {
           //XFillRectangle(X11dpy, X11Window, DefaultGC(X11dpy, X11screen), 20, 20, 10, 10);
        }
        if (ev.type == KeyPress)
           break;
     }

    XCloseDisplay(X11dpy);

    reg.tearDown();

  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
