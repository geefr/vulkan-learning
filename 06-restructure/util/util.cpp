#include "util.h"

#include <iostream>


#ifdef DEBUG
  vk::DispatchLoaderDynamic Util::mDidl;
  vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> Util::mDebugUtilsMessenger;
#endif
void Util::release() {
#ifdef DEBUG
  mDebugUtilsMessenger.release();
#endif
}

std::string Util::physicalDeviceTypeToString( vk::PhysicalDeviceType type ) {
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

std::string Util::vulkanAPIVersionToString( uint32_t version ) {
  auto major = VK_VERSION_MAJOR(version);
  auto minor = VK_VERSION_MINOR(version);
  auto patch = VK_VERSION_PATCH(version);
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

void Util::printPhysicalDeviceProperties( vk::PhysicalDevice& device ) {
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

void Util::printDetailedPhysicalDeviceInfo( vk::PhysicalDevice& device ) {
  vk::PhysicalDeviceMemoryProperties props;
  device.getMemoryProperties(&props);

  // Information on device memory
  std::cout << "== Device Memory ==" << "\n"
            << "Types : " << props.memoryTypeCount << "\n"
            << "Heaps : " << props.memoryHeapCount << "\n"
            << std::endl;

  for(auto i = 0u; i < props.memoryTypeCount; ++i )
  {
    auto memoryType = props.memoryTypes[i];
    if( (memoryType.propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible) ) {
      std::cout << "Type: " << i << ", Host visible: TRUE" << std::endl;
    } else {
      std::cout << "Type: " << i << ", Host visible: FALSE" << std::endl;
    }
  }
  std::cout << std::endl;
}

void Util::printQueueFamilyProperties( std::vector<vk::QueueFamilyProperties>& props ) {
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

VKAPI_ATTR VkBool32 VKAPI_CALL Util::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

  // TODO: Proper debug callback, this probably doesn't even work
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}


void Util::ensureExtension(const std::vector<vk::ExtensionProperties>& extensions, std::string extensionName) {
  if( std::find_if(extensions.begin(), extensions.end(), [&](auto& e) {
                   return e.extensionName == extensionName;
}) == extensions.end()) throw std::runtime_error("Extension not supported: " + extensionName);
}

uint32_t Util::findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags) {
  auto qFamProps = device.getQueueFamilyProperties();
  auto it = std::find_if(qFamProps.begin(), qFamProps.end(), [&](auto& p) {
    if( p.queueFlags & requiredFlags ) return true;
    return false;
  });
  if( it == qFamProps.end() ) return std::numeric_limits<uint32_t>::max();
  return it - qFamProps.begin();
}



void Util::initDidl( vk::Instance& instance ) {
  mDidl.init(instance, ::vkGetInstanceProcAddr);
}


void Util::initDebugMessenger( vk::Instance& instance ) {
  vk::DebugUtilsMessengerCreateInfoEXT cbInfo(
  {},
  {vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
   vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
   vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning},
  {vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation},
        debugCallback,
        nullptr
        );

  mDebugUtilsMessenger = instance.createDebugUtilsMessengerEXTUnique(cbInfo, nullptr, mDidl);
}
