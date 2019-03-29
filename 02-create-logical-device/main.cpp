
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>

vk::Instance createVulkanInstance()
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

  vk::Instance instance;
  if( vk::createInstance(
        &instanceCreateInfo, // Creation info
        nullptr, // Allocator - use the built-in one for now
        &instance // Output, the created instance
        // Dispatch loader optional - don't know what it does ;)
        ) != vk::Result::eSuccess ) {
    throw std::runtime_error("Failed to create Vulkan instance");
  }
  return instance;
}
vk::Device createLogicalDevice( vk::PhysicalDevice& physicalDevice, uint32_t queueFamily ) {

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

  vk::Device logicalDevice;
  if( physicalDevice.createDevice(&info, nullptr, &logicalDevice ) != vk::Result::eSuccess ) throw std::runtime_error("Failed to create logical device");
  return logicalDevice;
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

    auto physicalDevices = instance.enumeratePhysicalDevices();
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
    auto queueFamilyGraphics = std::find_if(queueFamilyProps.begin(), queueFamilyProps.end(), [&](auto& p) {
      if( p.queueFlags & vk::QueueFlagBits::eGraphics ) return true;
      return false;
    });
    if( queueFamilyGraphics == queueFamilyProps.end() ) throw std::runtime_error("Failed to find queue with graphics support");

    auto logicalDevice = createLogicalDevice( physicalDevice, static_cast<uint32_t>(queueFamilyGraphics - queueFamilyProps.begin()) );
    std::cout << "Created a logical device: " << std::hex << reinterpret_cast<uint64_t>(&logicalDevice) << std::endl;

  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
