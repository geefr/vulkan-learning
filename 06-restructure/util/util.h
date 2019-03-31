#ifndef UTIL_H
#define UTIL_H

#include <vulkan/vulkan.hpp>

class Util
{
public:
  static void release();

  static std::string physicalDeviceTypeToString( vk::PhysicalDeviceType type );
  static std::string vulkanAPIVersionToString( uint32_t version );
  static void printPhysicalDeviceProperties( vk::PhysicalDevice& device );
  static void printDetailedPhysicalDeviceInfo( vk::PhysicalDevice& device );
  static void printQueueFamilyProperties( std::vector<vk::QueueFamilyProperties>& props );
  static void ensureExtension(const std::vector<vk::ExtensionProperties>& extensions, std::string extensionName);
  static uint32_t findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags);

#ifdef DEBUG
  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
      VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
      VkDebugUtilsMessageTypeFlagsEXT messageType,
      const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
      void* pUserData);

  static void initDidl(vk::Instance& instance);
  static void initDebugMessenger( vk::Instance& instance );

  static vk::DispatchLoaderDynamic mDidl;
  static vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> mDebugUtilsMessenger;
#endif
};

#endif // UTIL_H
