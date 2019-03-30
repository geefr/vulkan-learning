
#include "registrar.h"

#include <iostream>

Registrar Registrar::mReg;

Registrar::Registrar()
{
}

Registrar::~Registrar()
{

}

void Registrar::tearDown() {

  // TODO: Not sure if all this is needed?
  // Initially thought the smart pointers would
  // be best but might be hard to sort out
  // cleanup order in that case..
  mDevice->waitIdle();

  for( auto& iv : mSwapChainImageViews ) iv.release();

  mSwapChain.release();
  mSurface.release();
  for( auto& p : mCommandPools ) p.release();
  mDevice.release();

  mDebugUtilsMessenger.release();

  mInstance.release();

}

void Registrar::ensureExtension(const std::vector<vk::ExtensionProperties>& extensions, std::string extensionName) {
  if( std::find_if(extensions.begin(), extensions.end(), [&](auto& e) {
    return e.extensionName == extensionName;
  }) == extensions.end()) throw std::runtime_error("Extension not supported: " + extensionName);
}

vk::Instance& Registrar::createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer) {
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

  for( auto& e : enabledInstanceExtensions ) ensureExtension(supportedExtensions, e);
  vk::InstanceCreateInfo instanceCreateInfo(
        vk::InstanceCreateFlags(),
        &applicationInfo, // Application Info
        enabledLayerCount, // enabledLayerCount - don't need any validation layers right now
        enabledLayerNames, // ppEnabledLayerNames
        static_cast<uint32_t>(enabledInstanceExtensions.size()), // enabledExtensionCount. In vulkan these need to be requested on init unlike in gl
        enabledInstanceExtensions.data() // ppEnabledExtensionNames
        );

  mInstance = vk::createInstanceUnique(instanceCreateInfo);

#ifdef DEBUG
  mDidl.init(mInstance.get(), ::vkGetInstanceProcAddr);

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

  mDebugUtilsMessenger = mInstance->createDebugUtilsMessengerEXTUnique(cbInfo, nullptr, mDidl);
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

vk::Device& Registrar::createLogicalDevice(vk::QueueFlags qFlags ) {
  mQueueFamIndex = findQueue(mPhysicalDevices.front(), qFlags);
  return createLogicalDevice();
}

vk::Device& Registrar::createLogicalDevice() {
  if( mQueueFamIndex > mPhysicalDevices.size() ) throw std::runtime_error("createLogicalDevice: Physical device doesn't support required queue types");

  auto supportedExtensions = mPhysicalDevices.front().enumerateDeviceExtensionProperties();
  std::vector<const char*> enabledDeviceExtensions;
#if defined(VK_USE_PLATFORM_WIN32_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_LIB_XCB_KHR) || defined(USE_GLFW)
  enabledDeviceExtensions.push_back("VK_KHR_swapchain");
  for( auto& e : enabledDeviceExtensions ) ensureExtension(supportedExtensions, e);
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

vk::CommandPool& Registrar::createCommandPool( vk::CommandPoolCreateFlags flags ) {
  vk::CommandPoolCreateInfo info(flags, mQueueFamIndex);
  mCommandPools.emplace_back(mDevice->createCommandPoolUnique(info));
  return mCommandPools.back().get();
}

vk::UniqueBuffer Registrar::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags ) {
  // Exclusive buffer of size, for usageFlags
  vk::BufferCreateInfo info(
        vk::BufferCreateFlags(),
        size,
        usageFlags
        );
  return mDevice->createBufferUnique(info);
}

/// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
uint32_t Registrar::selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags ) {
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
vk::UniqueDeviceMemory Registrar::allocateDeviceMemoryForBuffer( vk::Buffer& buffer, vk::MemoryPropertyFlags userReqs ) {
  // Find out what kind of memory the buffer needs
  vk::MemoryRequirements memReq = mDevice->getBufferMemoryRequirements(buffer);

  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );

  vk::MemoryAllocateInfo info(
        memReq.size,
        heapIdx);

  return mDevice->allocateMemoryUnique(info);
}

/// Bind memory to a buffer
void Registrar::bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset) {
  mDevice->bindBufferMemory(buffer, memory, offset);
}

/// Map a region of device memory to host memory
void* Registrar::mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size ) {
  return mDevice->mapMemory(deviceMem, offset, size);
}

/// Unmap a region of device memory
void Registrar::unmapMemory( vk::DeviceMemory& deviceMem ) {
  mDevice->unmapMemory(deviceMem);
}

/// Flush memory/caches
void Registrar::flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem ) {
  mDevice->flushMappedMemoryRanges(mem.size(), mem.data());
}

uint32_t Registrar::findQueue(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags) {
  auto qFamProps = device.getQueueFamilyProperties();
  auto it = std::find_if(qFamProps.begin(), qFamProps.end(), [&](auto& p) {
    if( p.queueFlags & requiredFlags ) return true;
    return false;
  });
  if( it == qFamProps.end() ) return std::numeric_limits<uint32_t>::max();
  return it - qFamProps.begin();
}

#ifdef VK_USE_PLATFORM_XLIB_KHR
uint32_t Registrar::findPresentQueueXlib(vk::PhysicalDevice& device, vk::QueueFlags requiredFlags, Display* dpy, VisualID vid)
{
  auto qFamProps = device.getQueueFamilyProperties();

  uint32_t qIdx = std::numeric_limits<uint32_t>::max();
  for( auto i = 0u; i < qFamProps.size(); ++i ) {
    auto& p = qFamProps[i];
    if( !(p.queueFlags & requiredFlags) ) continue;
    if( device.getXlibPresentationSupportKHR(i, dpy, vid)) {
      qIdx = i;
      break;
    }
  }
  return qIdx;
}

vk::Device& Registrar::createLogicalDeviceWithPresentQueueXlib(vk::QueueFlags qFlags, Display* dpy, VisualID vid) {
  mQueueFamIndex = findPresentQueueXlib( mPhysicalDevices.front(), qFlags, dpy, vid);

  auto& result = createLogicalDevice();

  return result;
}

vk::SurfaceKHR& Registrar::createSurfaceXlib(Display* dpy, Window window) {
  vk::XlibSurfaceCreateInfoKHR info(
        vk::XlibSurfaceCreateFlagsKHR(),
        dpy,
        window
        );

  mSurface = mInstance->createXlibSurfaceKHRUnique(info);
  return mSurface.get();
}
#endif

#ifdef USE_GLFW
vk::SurfaceKHR& Registrar::createSurfaceGLFW(GLFWwindow* window) {
  VkSurfaceKHR surface;
  if(glfwCreateWindowSurface(mInstance.get(), window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("createSurfaceGLFW: Failed to create window surface");
  }

  mSurface.reset(surface);
  return mSurface.get();
}
#endif


vk::SwapchainKHR& Registrar::createSwapChain() {
  if( !mPhysicalDevices.front().getSurfaceSupportKHR(mQueueFamIndex, mSurface.get()) ) throw std::runtime_error("createSwapChainXlib: Physical device doesn't support surfaces");

  // Parameters used in swapchain must comply with limits of the surface
  auto caps = mPhysicalDevices.front().getSurfaceCapabilitiesKHR(mSurface.get());

  // First the number of images (none, double, triple buffered)
  auto numImages = 3u;
  while( numImages > caps.maxImageCount ) numImages--;
  if( numImages != 3u ) std::cerr << "Creating swapchain with " << numImages << " images" << std::endl;
  if( numImages < caps.minImageCount ) throw std::runtime_error("Unable to create swapchain, invalid num images");

  // Ideally we want full alpha support
  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::ePostMultiplied; // TODO: Pre or post? can't remember the difference
  if( !(caps.supportedCompositeAlpha & alphaMode) ) {
    std::cerr << "Surface doesn't support full alpha, falling back" << std::endl;
    alphaMode = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  }

  auto imageUsage = vk::ImageUsageFlags(vk::ImageUsageFlagBits::eColorAttachment);
  if( !(caps.supportedUsageFlags & imageUsage) ) throw std::runtime_error("Surface doesn't support color attachment");

  // caps.maxImageArrayLayers;
  // caps.minImageExtent;
  // caps.currentTransform;
  // caps.supportedTransforms;

  // Choose a pixel format
  auto surfaceFormats = mPhysicalDevices.front().getSurfaceFormatsKHR(mSurface.get());
  // TODO: Just choosing the first one here..
  mSwapChainFormat = surfaceFormats.front().format;
  auto chosenColourSpace = surfaceFormats.front().colorSpace;

  vk::SwapchainCreateInfoKHR info(
        vk::SwapchainCreateFlagsKHR(),
        mSurface.get(),
        numImages, // Num images in the swapchain
        mSwapChainFormat, // Pixel format
        chosenColourSpace, // Colour space
        caps.currentExtent, // Size of window
        1, // Image array layers
        imageUsage, // Image usage flags
        vk::SharingMode::eExclusive, 0, nullptr, // Exclusive use by one queue
        caps.currentTransform, // flip/rotate before presentation
        alphaMode, // How alpha is mapped
        vk::PresentModeKHR::eFifo, // vsync (mailbox is vsync or faster, fifo is vsync)
        true, // Clipped - For cases where part of window doesn't need to be rendered/is offscreen
        vk::SwapchainKHR() // The old swapchain to delete
        );

  mSwapChain = mDevice->createSwapchainKHRUnique(info);

  mSwapChainImages = mDevice->getSwapchainImagesKHR(mSwapChain.get());

  return mSwapChain.get();
}

void Registrar::createSwapChainImageViews() {
  for( auto i = 0u; i < mSwapChainImages.size(); ++i ) {
    vk::ImageViewCreateInfo info(
          {}, // empty flags
          mSwapChainImages[i],
          vk::ImageViewType::e2D,
          mSwapChainFormat,
          {}, // IDENTITY component mapping
          vk::ImageSubresourceRange(
          {vk::ImageAspectFlagBits::eColor},0, 1, 0, 1)
          );
    mSwapChainImageViews.emplace_back( mDevice->createImageViewUnique(info) );
  }
}

std::string Registrar::physicalDeviceTypeToString( vk::PhysicalDeviceType type ) {
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
std::string Registrar::vulkanAPIVersionToString( uint32_t version ) {
  auto major = VK_VERSION_MAJOR(version);
  auto minor = VK_VERSION_MINOR(version);
  auto patch = VK_VERSION_PATCH(version);
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}
void Registrar::printPhysicalDeviceProperties( vk::PhysicalDevice& device ) {
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
void Registrar::printDetailedPhysicalDeviceInfo( vk::PhysicalDevice& device ) {
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
void Registrar::printQueueFamilyProperties( std::vector<vk::QueueFamilyProperties>& props ) {
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

VKAPI_ATTR VkBool32 VKAPI_CALL Registrar::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

  // TODO: Proper debug callback, this probably doesn't even work
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}
