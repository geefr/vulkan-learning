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

  vk::Instance& createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer = VK_API_VERSION_1_0);
  vk::Device& createLogicalDevice(vk::QueueFlags qFlags);
  uint32_t findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags);
#ifdef VK_USE_PLATFORM_WIN32_KHR
#error "Win32 support not implemented"
  uint32_t findPresentQueueWin32(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags)
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
  uint32_t findPresentQueueXlib(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags, Display* dpy, VisualID vid);
  vk::Device& createLogicalDeviceWithPresentQueueXlib(vk::QueueFlags qFlags, Display* dpy, VisualID vid);
  vk::SurfaceKHR& createSurfaceXlib(Display* dpy, Window window);
#endif
#ifdef VK_USE_PLATFORM_LIB_XCB_KHR
#error "XCB support not implemented"
  uint32_t findPresentQueueXcb(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags);
#endif
#ifdef USE_GLFW
  vk::SurfaceKHR& createSurfaceGLFW(GLFWwindow* window);
#endif
  vk::SwapchainKHR& createSwapChain();
  void createSwapChainImageViews();
  void createRenderPass();
  void createGraphicsPipeline();
  void createFramebuffers();

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
  vk::SwapchainKHR& swapChain();
  std::vector<vk::Image>& swapChainImages();
  std::vector<vk::UniqueCommandPool>& commandPools();
  vk::RenderPass& renderPass();
  std::vector<vk::UniqueFramebuffer>& frameBuffers();
  vk::Extent2D& swapChainExtent();
  vk::UniquePipeline& graphicsPipeline();
private:
  Registrar();
  Registrar(const Registrar&) = delete;
  Registrar(const Registrar&&) = delete;
  ~Registrar();

  std::vector<char> readFile(const std::string& fileName);
  vk::UniqueShaderModule createShaderModule(const std::string& fileName);

  static Registrar mReg;

  /// Don't call unless mQueueFamIndex is set
  vk::Device& createLogicalDevice();

  vk::UniqueInstance mInstance;
  std::vector<vk::PhysicalDevice> mPhysicalDevices;

  // TODO: Initially just 1 logical device and 1 queue
  vk::UniqueDevice mDevice;
  vk::UniqueSurfaceKHR mSurface;
  vk::UniqueSwapchainKHR mSwapChain;
  vk::Format mSwapChainFormat;
  vk::Extent2D mSwapChainExtent;
  std::vector<vk::Image> mSwapChainImages;
  std::vector<vk::UniqueImageView> mSwapChainImageViews;
  /// One framebuffer definition for each swap chain image, as we'll cycle through them each frame
  std::vector<vk::UniqueFramebuffer> mSwapChainFrameBuffers;
  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniqueRenderPass mRenderPass;
  vk::UniquePipeline mGraphicsPipeline;

  uint32_t mQueueFamIndex = std::numeric_limits<uint32_t>::max();
  std::vector<vk::Queue> mQueues;
  std::vector<vk::UniqueCommandPool> mCommandPools;


};

inline Registrar& Registrar::singleton() { return mReg; }
inline vk::Instance& Registrar::instance() { return mInstance.get(); }
inline vk::Device& Registrar::device() { return mDevice.get(); }
inline std::vector<vk::PhysicalDevice>& Registrar::physicalDevices() { return mPhysicalDevices; }
inline vk::PhysicalDevice& Registrar::physicalDevice() { return mPhysicalDevices.front(); }
inline uint32_t Registrar::queueFamilyIndex() { return mQueueFamIndex; }
inline vk::Queue& Registrar::queue() { return mQueues.front(); }
inline vk::SwapchainKHR& Registrar::swapChain() { return mSwapChain.get(); }
inline std::vector<vk::Image>& Registrar::swapChainImages() { return mSwapChainImages; }
inline std::vector<vk::UniqueCommandPool>& Registrar::commandPools() { return mCommandPools; }
inline vk::RenderPass& Registrar::renderPass() { return mRenderPass.get(); }
inline std::vector<vk::UniqueFramebuffer>& Registrar::frameBuffers() { return mSwapChainFrameBuffers; }
inline vk::Extent2D& Registrar::swapChainExtent() { return mSwapChainExtent; }
inline vk::UniquePipeline& Registrar::graphicsPipeline() { return mGraphicsPipeline; }
#endif
