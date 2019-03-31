#include "deviceinstance.h"

#include "util.h"

AppDeviceInstance::AppDeviceInstance(
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

vk::Instance& AppDeviceInstance::createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer) {
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

vk::Device& AppDeviceInstance::createLogicalDevice(vk::QueueFlags qFlags ) {
  mQueueFamIndex = Util::findQueue(mPhysicalDevices.front(), qFlags);
  return createLogicalDevice();
}

vk::Device& AppDeviceInstance::createLogicalDevice() {
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

void AppDeviceInstance::waitAllDevicesIdle() {
  mDevice->waitIdle();
}
