
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

  vk::InstanceCreateInfo instanceCreateInfo(
        vk::InstanceCreateFlags(),
        &applicationInfo, // Application Info
        0, // enabledLayerCount - don't need any validation layers right now
        nullptr, // ppEnabledLayerNames
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

void printPhysicalDeviceProperties( vk::PhysicalDevice& device )
{
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


int main(void)
{
  try {
    auto instance = createVulkanInstance();

    auto physicalDevices = instance.enumeratePhysicalDevices();
    if( physicalDevices.empty() ) throw std::runtime_error("Failed to enumerate physical devices");

    // Great we found some devices, let's see what the hardware can do
    std::cout << "Found " << physicalDevices.size() << " physical devices" << std::endl;
    for( auto& p : physicalDevices ) printPhysicalDeviceProperties(p);
  } catch ( std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
