
// https://github.com/KhronosGroup/Vulkan-Hpp
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>

vk::UniqueInstance createVulkanInstance()
{
  vk::ApplicationInfo applicationInfo(
        "01-initialisation", // Application Name
        0, // Application version
        "geefr", // engine name
        0, // engine version
        VK_API_VERSION_1_0 // absolute minimum vulkan api
        );

#ifdef DEBUG
  uint32_t enabledLayerCount = 1;
  const char* const enabledLayerNames[] = {
    "VK_LAYER_LUNARG_standard_validation",
  };
#else
  uint32_t enabledLayerCount = 0;
  const char* const* enabledLayerNames = nullptr;
#endif

  vk::InstanceCreateInfo instanceCreateInfo(
        vk::InstanceCreateFlags(),
        &applicationInfo, // Application Info
        enabledLayerCount, // enabledLayerCount - don't need any validation layers right now
        enabledLayerNames, // ppEnabledLayerNames
        0, // enabledExtensionCount - don't need any extensions yet. In vulkan these need to be requested on init unlike in gl
        nullptr // ppEnabledExtensionNames
        );

  vk::UniqueInstance instance(vk::createInstanceUnique(instanceCreateInfo));
  return instance;
}
vk::UniqueDevice createLogicalDevice( vk::PhysicalDevice& physicalDevice, uint32_t queueFamily ) {

  vk::DeviceQueueCreateInfo queueInfo(
        vk::DeviceQueueCreateFlags(),
        queueFamily, // Queue family index
        1, // Number of queues to create
        nullptr // Priorities of each queue
        );

  // The features of the physical device
  vk::PhysicalDeviceFeatures deviceSupportedFeatures;
  physicalDevice.getFeatures(&deviceSupportedFeatures);

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
        0, // Enabled layer count
        nullptr, // Enabled layers
        0, // Enabled extension count
        nullptr, // Enabled extensions
        &deviceRequiredFeatures // Physical device features
        );

  vk::UniqueDevice logicalDevice( physicalDevice.createDeviceUnique(info));
  return logicalDevice;
}

/// Populate a command buffer in order to copy data from source to dest
void fillCB_CopyBufferObjects( vk::CommandBuffer& commandBuffer, vk::Buffer& src, vk::Buffer& dst, vk::DeviceSize srcOff = 0, vk::DeviceSize dstOff = 0, vk::DeviceSize size = 0 )
{
  // Start the buffer
  // Default flags
  // - We might execute this buffer more than once
  // - We won't execute it multiple times simultaneously
  // - We're not creating a secondary command buffer
  vk::CommandBufferBeginInfo beginInfo;
  commandBuffer.begin(&beginInfo);

  //vk::BufferCopy cpy(srcOff, dstOff, size);
  vk::ArrayProxy<vk::BufferCopy> cpy = {{srcOff, dstOff, size}};
  commandBuffer.copyBuffer(src, dst, cpy.size(), cpy.data());


  // End the buffer
  commandBuffer.end();
}

std::string physicalDeviceTypeToString( vk::PhysicalDeviceType type ) {
  switch(type)
  {
    case vk::PhysicalDeviceType::eCpu: return "CPU";
    case vk::PhysicalDeviceType::eDiscreteGpu: return "Discrete GPU";
    case vk::PhysicalDeviceType::eIntegratedGpu: return "Integrated GPU";
    case vk::PhysicalDeviceType::eOther: return "Other";
    case vk::PhysicalDeviceType::eVirtualGpu: return "Virtual GPU";
  }
  return "Unknown";
};
std::string vulkanAPIVersionToString( uint32_t version ) {
  auto major = VK_VERSION_MAJOR(version);
  auto minor = VK_VERSION_MINOR(version);
  auto patch = VK_VERSION_PATCH(version);
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}
void printPhysicalDeviceProperties( vk::PhysicalDevice& device ) {
  vk::PhysicalDeviceProperties props;
  device.getProperties(&props);

  std::cout << "==== Physical Device Properties ====" << "\n"
            << "API Version    :" << vulkanAPIVersionToString(props.apiVersion) << "\n"
            << "Driver Version :" << props.driverVersion << "\n"
            << "Vendor ID      :" << props.vendorID << "\n"
            << "Device Type    :" << physicalDeviceTypeToString(props.deviceType) << "\n"
            << "Device Name    :" << props.deviceName << "\n" // Just incase it's not nul-terminated
            // pipeline cache uuid
            // physical device limits
            // physical device sparse properties
            << std::endl;
}
void printDetailedPhysicalDeviceInfo( vk::PhysicalDevice& device ) {
  vk::PhysicalDeviceMemoryProperties props;
  device.getMemoryProperties(&props);

  // Information on device memory
  std::cout << "== Device Memory ==" << "\n"
            << "Types : " << props.memoryTypeCount << "\n"
            << "Heaps : " << props.memoryHeapCount << "\n"
            << std::endl;

}
void printQueueFamilyProperties( std::vector<vk::QueueFamilyProperties>& props ) {
  std::cout << "== Queue Family Properties ==" << std::endl;
  auto i = 0u;
  for( auto& queueFamily : props ) {
    std::cout << "Queue Family  : " << i++ << "\n"
              << "Queue Count   : " << queueFamily.queueCount << "\n"
              << "Graphics      : " << static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) << "\n"
              << "Compute       : " << static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) << "\n"
              << "Transfer      : " << static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) << "\n"
              << "Sparse Binding: " << static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eSparseBinding) << "\n"
              << std::endl;

  }
}

int main(int argc, char* argv[])
{
  try {
    auto instance = createVulkanInstance();

    auto physicalDevices = instance->enumeratePhysicalDevices();
    if( physicalDevices.empty() ) throw std::runtime_error("Failed to enumerate physical devices");

    // Device order in the list isn't guaranteed, likely the integrated gpu is first
    // So why don't we fix that? As we're using the C++ api we can just sort the vector
    std::sort(physicalDevices.begin(), physicalDevices.end(), [&](auto& a, auto& b) {
      vk::PhysicalDeviceProperties propsA;
      a.getProperties(&propsA);
      vk::PhysicalDeviceProperties propsB;
      a.getProperties(&propsB);
      // For now just ensure discrete GPUs are first, then order by support vulkan api version
      if( propsA.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ) return true;
      if( propsA.apiVersion > propsB.apiVersion ) return true;
      return false;
    });

    // Great we found some devices, let's see what the hardware can do
    std::cout << "Found " << physicalDevices.size() << " physical devices (First will be used from here on)" << std::endl;
    for( auto& p : physicalDevices ) printPhysicalDeviceProperties(p);

    // Let's print some further info about a device
    // The first device should now be the 'best'
    auto physicalDevice = physicalDevices.front();
    printDetailedPhysicalDeviceInfo(physicalDevice);

    // Find out what queues are available
    auto queueFamilyProps = physicalDevice.getQueueFamilyProperties();
    printQueueFamilyProperties(queueFamilyProps);

    // Create a logical device to interact with
    // To do this we also need to specify how many queues from which families we want to create
    // In this case just 1 queue from the first family which supports graphics
    auto qFamilyGraphics = std::find_if(queueFamilyProps.begin(), queueFamilyProps.end(), [&](auto& p) {
      if( p.queueFlags & vk::QueueFlagBits::eGraphics ) return true;
      return false;
    });
    if( qFamilyGraphics == queueFamilyProps.end() ) throw std::runtime_error("Failed to find queue with graphics support");
    auto qFamilyGraphicsIdx = static_cast<uint32_t>(qFamilyGraphics - queueFamilyProps.begin());

    auto logicalDevice = createLogicalDevice( physicalDevice, qFamilyGraphicsIdx );
    std::cout << "Created a logical device: " << std::hex << reinterpret_cast<uint64_t>(&(logicalDevice.get())) << std::endl;

    // Get the queue from the device
    // queues are the thing that actually do the work
    // could be a subprocessor on the device, or some other subsection of capability
    vk::Queue q = logicalDevice->getQueue(qFamilyGraphicsIdx, 0);

    // A pool for allocating command buffers
    // Flags here are flexible, but may add a small overhead
    vk::CommandPoolCreateInfo commandPoolCreateInfo(
          vk::CommandPoolCreateFlags(
            vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer // Buffers can be reset individually, instead of needing to reset the entire pool
            ),
          qFamilyGraphicsIdx);
    auto commandPool = logicalDevice->createCommandPoolUnique(commandPoolCreateInfo);

    // Now make a command buffer
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo(
          commandPool.get(), // The pool to allocate from
          vk::CommandBufferLevel::ePrimary, // A primary buffer (Secondary buffers are ones that can be called by other command buffers)
          1 // Just the one for now
          );

    auto commandBuffers = logicalDevice->allocateCommandBuffersUnique(commandBufferAllocateInfo);
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
    logicalDevice->resetCommandPool(commandPool.get(), vk::CommandPoolResetFlagBits::eReleaseResources);

    // Fill the command buffer with our commands
    // TODO: Shouldn't have skimmed that chapter on allocating buffers should ye?
    // fillCB_CopyBufferObjects(commandBuffer, srcBuffer, dstBuffer);



    // Wait for everything to finish before cleanup
    // Cleanup handled by the smart handles in this case
    logicalDevice->waitIdle();
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
