#ifndef REGISTRAR_H
#define REGISTRAR_H

#include <vulkan/vulkan.hpp>

/**
 * Probably the wrong name for this
 * Basically a big singleton for storing
 * vulkan objects in, and a bunch
 * of util functions
 *
 * Each instance of this class maps to a vulkan instance
 * So for now it's a singleton
 */

class Registrar
{
public:
  static Registrar& singleton();

  vk::Instance& createVulkanInstance(std::string appName, uint32_t appVer, uint32_t apiVer = VK_API_VERSION_1_0);
  vk::Device& createLogicalDevice(vk::QueueFlags qFlags);
  uint32_t findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags);
  vk::CommandPool& createCommandPool( vk::CommandPoolCreateFlags flags );
  vk::UniqueBuffer createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags );
  /// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
  uint32_t selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags );
  /// Allocate device memory suitable for the specified buffer
  vk::UniqueDeviceMemory allocateDeviceMemoryForBuffer( vk::Buffer& buffer, vk::MemoryPropertyFlags userReqs = {} );
  /// Bind memory to a buffer
  void bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset = 0);
  /// Map a region of device memory to host memory
  void* mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size );
  /// Unmap a region of device memory
  void unmapMemory( vk::DeviceMemory& deviceMem );
  /// Flush memory/caches
  void flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem );




  vk::Instance& instance();
  vk::Device& device();
  std::vector<vk::PhysicalDevice>& physicalDevices();
  vk::PhysicalDevice& physicalDevice();
  uint32_t queueFamilyIndex();
  vk::Queue& queue();
  std::vector<vk::UniqueCommandPool>& commandPools();

  std::string physicalDeviceTypeToString( vk::PhysicalDeviceType type );
  std::string vulkanAPIVersionToString( uint32_t version );
  void printPhysicalDeviceProperties( vk::PhysicalDevice& device );
  void printDetailedPhysicalDeviceInfo( vk::PhysicalDevice& device );
  void printQueueFamilyProperties( std::vector<vk::QueueFamilyProperties>& props );

private:
  Registrar();
  Registrar(const Registrar&) = delete;
  Registrar(const Registrar&&) = delete;
  ~Registrar() = default;

  static Registrar mReg;

  vk::UniqueInstance mInstance;
  std::vector<vk::PhysicalDevice> mPhysicalDevices;
  std::vector<vk::UniqueCommandPool> mCommandPools;

  // TODO: Initially just 1 logical device and 1 queue
  vk::UniqueDevice mDevice;
  uint32_t mQueueFamIndex = std::numeric_limits<uint32_t>::max();
  std::vector<vk::Queue> mQueues;
};

inline Registrar& Registrar::singleton() { return mReg; }
inline vk::Instance& Registrar::instance() { return mInstance.get(); }
inline vk::Device& Registrar::device() { return mDevice.get(); }
inline std::vector<vk::PhysicalDevice>& Registrar::physicalDevices() { return mPhysicalDevices; }
inline vk::PhysicalDevice& Registrar::physicalDevice() { return mPhysicalDevices.front(); }
inline uint32_t Registrar::queueFamilyIndex() { return mQueueFamIndex; }
inline vk::Queue& Registrar::queue() { return mQueues.front(); }
inline std::vector<vk::UniqueCommandPool>& Registrar::commandPools() { return mCommandPools; }
#endif