#include "deviceinstance.h"

#include "util.h"

DeviceInstance::DeviceInstance(
    const std::vector<const char*>& requiredInstanceExtensions,
    const std::vector<const char*>& requiredDeviceExtensions,
    const std::string& appName,
    uint32_t appVer,
    uint32_t vulkanApiVer,
    vk::QueueFlags qFlags) {
  createVulkanInstance(requiredInstanceExtensions, appName, appVer, vulkanApiVer);
  // TODO: Need to split device and queue creation apart
  createLogicalDevice(qFlags);
}

vk::Instance& DeviceInstance::createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer) {
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
        enabledLayerCount, // enabledLayerCount
        enabledLayerNames, // ppEnabledLayerNames
        static_cast<uint32_t>(enabledInstanceExtensions.size()), // enabledExtensionCount. In vulkan these need to be requested on init unlike in gl
        enabledInstanceExtensions.data() // ppEnabledExtensionNames
        );

  mInstance = vk::createInstanceUnique(instanceCreateInfo);

#ifdef DEBUG
  // Now we have an instance we can register the debug callback
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

vk::Device& DeviceInstance::createLogicalDevice(vk::QueueFlags qFlags ) {
  mQueueFamIndex = Util::findQueue(*this, qFlags);
  return createLogicalDevice();
}

vk::Device& DeviceInstance::createLogicalDevice() {
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

void DeviceInstance::waitAllDevicesIdle() {
  mDevice->waitIdle();
}


vk::UniqueCommandPool DeviceInstance::createCommandPool( vk::CommandPoolCreateFlags flags ) {
  vk::CommandPoolCreateInfo info(flags, mQueueFamIndex); // TODO: Hack! should be passed by caller/some reference to the command queue class we don't have yet
  return mDevice->createCommandPoolUnique(info);
}

vk::UniqueBuffer DeviceInstance::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags ) {
  // Exclusive buffer of size, for usageFlags
  vk::BufferCreateInfo info(
        vk::BufferCreateFlags(),
        size,
        usageFlags
        );
  return mDevice->createBufferUnique(info);
}

/// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
uint32_t DeviceInstance::selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags ) {
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
vk::UniqueDeviceMemory DeviceInstance::allocateDeviceMemoryForBuffer( vk::Buffer& buffer, vk::MemoryPropertyFlags userReqs ) {
  // Find out what kind of memory the buffer needs
  vk::MemoryRequirements memReq = mDevice->getBufferMemoryRequirements(buffer);

  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );

  vk::MemoryAllocateInfo info(
        memReq.size,
        heapIdx);

  return mDevice->allocateMemoryUnique(info);
}

/// Bind memory to a buffer
void DeviceInstance::bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset) {
  mDevice->bindBufferMemory(buffer, memory, offset);
}

/// Map a region of device memory to host memory
void* DeviceInstance::mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size ) {
  return mDevice->mapMemory(deviceMem, offset, size);
}

/// Unmap a region of device memory
void DeviceInstance::unmapMemory( vk::DeviceMemory& deviceMem ) {
  mDevice->unmapMemory(deviceMem);
}

/// Flush memory/caches
void DeviceInstance::flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem ) {
  mDevice->flushMappedMemoryRanges(mem.size(), mem.data());
}

