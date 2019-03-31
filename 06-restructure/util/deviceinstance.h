#ifndef APPDEVICEINSTANCE_H
#define APPDEVICEINSTANCE_H

#include <vulkan/vulkan.hpp>

#include <vector>

/**
 * Base instance/device information for the application
 * - The vulkan instance
 * - Selection of a physical device
 * - creation of logical device
 */
class AppDeviceInstance
{
public:
  AppDeviceInstance() = delete;
  AppDeviceInstance(const AppDeviceInstance&) = delete;
  AppDeviceInstance(AppDeviceInstance&&) = delete;
  ~AppDeviceInstance() = default;

  /**
   * The everything constructor
   *
   * TODO: To use this you need to know what you want in the first place including what physical devices are on the system
   * There should be callbacks and such to let the application interact with the startup process and make decisions based
   * on information that's unknown when this class starts construction
   */
  AppDeviceInstance(
      const std::vector<const char*>& requiredInstanceExtensions,
      const std::vector<const char*>& requiredDeviceExtensions,
      const std::string& appName,
      uint32_t appVer,
      uint32_t vulkanApiVer = VK_API_VERSION_1_0,
      vk::QueueFlags qFlags = vk::QueueFlagBits::eGraphics);


  vk::Instance& instance() { return mInstance.get(); }
  vk::Device& device() { return mDevice.get(); }
  std::vector<vk::PhysicalDevice>& physicalDevices() { return mPhysicalDevices; }
  vk::PhysicalDevice& physicalDevice() { return mPhysicalDevices.front(); }
  uint32_t queueFamilyIndex() { return mQueueFamIndex; }
  vk::Queue& queue() { return mQueues.front(); }

  /// Wait until all physical devices are idle
  void waitAllDevicesIdle();

private:
  vk::Instance& createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer = VK_API_VERSION_1_0);
  vk::Device& createLogicalDevice(vk::QueueFlags qFlags);
  vk::Device& createLogicalDevice();


  std::vector<vk::PhysicalDevice> mPhysicalDevices;

  vk::UniqueInstance mInstance;
  vk::UniqueDevice mDevice;
  // TODO: Initially a single queue chosen based on init
  // Queue really should be separated from this bit, or at least more flexible
  // to allow multiple named queues
  uint32_t mQueueFamIndex = std::numeric_limits<uint32_t>::max();
  std::vector<vk::Queue> mQueues;
};

#endif // APPDEVICEINSTANCE_H
