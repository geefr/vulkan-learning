#ifndef REGISTRAR_H
#define REGISTRAR_H

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#endif
// https://github.com/KhronosGroup/Vulkan-Hpp
# include <vulkan/vulkan.hpp>



#ifdef VK_USE_PLATFORM_XLIB_KHR
# include <X11/Xlib.h>
#endif

#include "deviceinstance.h"
#include "windowintegration.h"
#include "framebuffer.h"

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

  void tearDown();

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

  void createRenderPass();
  void createGraphicsPipeline();

  std::vector<vk::UniqueCommandPool>& commandPools();
  vk::RenderPass& renderPass();
  vk::UniquePipeline& graphicsPipeline();

  // Our classyboys to obfuscate the verbosity somewhat
  // Public in reg for now as reg is doing the cleanup right now..
  std::unique_ptr<AppDeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;

private:
  Registrar();
  Registrar(const Registrar&) = delete;
  Registrar(const Registrar&&) = delete;
  ~Registrar();

  std::vector<char> readFile(const std::string& fileName);
  vk::UniqueShaderModule createShaderModule(const std::string& fileName);

  static Registrar mReg;

  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniqueRenderPass mRenderPass;
  vk::UniquePipeline mGraphicsPipeline;

  std::vector<vk::UniqueCommandPool> mCommandPools;
};

inline Registrar& Registrar::singleton() { return mReg; }
inline vk::UniquePipeline& Registrar::graphicsPipeline() { return mGraphicsPipeline; }
inline vk::RenderPass& Registrar::renderPass() { return mRenderPass.get(); }
#endif
