/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#include "deviceinstance.h"

#include "util.h"

DeviceInstance::DeviceInstance(
    const std::vector<const char*>& requiredInstanceExtensions,
    const std::vector<const char*>& requiredDeviceExtensions,
    const std::string& appName,
    uint32_t appVer,
    uint32_t vulkanApiVer,
    std::vector<vk::QueueFlags> qFlags,
    const std::vector<const char*>& enabledLayers) {
  createVulkanInstance(requiredInstanceExtensions, appName, appVer, vulkanApiVer, enabledLayers);
  // TODO: Need to split device and queue creation apart
  createLogicalDevice(qFlags);
}

DeviceInstance::~DeviceInstance() {
  // Make sure the debug callback has been cleaned up before the vulkan instance
  mDevice.reset();
  Util::reset();
  mInstance.reset();
}

void DeviceInstance::createVulkanInstance(const std::vector<const char*>& requiredExtensions, std::string appName, uint32_t appVer, uint32_t apiVer, const std::vector<const char*>& enabledLayers) {
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
  auto applicationInfo = vk::ApplicationInfo()
      .setPApplicationName(appName.c_str())
      .setApplicationVersion(appVer)
      .setPEngineName("Vulkan Utils by Gareth Francis (geefr) (gfrancis.dev@gmail.com)")
      .setEngineVersion(1)
      .setApiVersion(apiVer);

  auto instanceLayers = enabledLayers;
#ifdef ENABLE_VK_DEBUG
  // instanceLayers.push_back("VK_LAYER_LUNARG_api_dump");
  instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
  enabledInstanceExtensions.emplace_back("VK_EXT_debug_utils");
#endif

  auto numLayers = static_cast<uint32_t>(instanceLayers.size());
  for( auto& e : enabledInstanceExtensions ) Util::ensureExtension(supportedExtensions, e);

  auto instanceCreateInfo = vk::InstanceCreateInfo()
      .setFlags({})
      .setPApplicationInfo(&applicationInfo)
      .setEnabledLayerCount(numLayers)
      .setPpEnabledLayerNames(numLayers ? instanceLayers.data() : nullptr)
      .setEnabledExtensionCount(static_cast<uint32_t>(enabledInstanceExtensions.size()))
      .setPpEnabledExtensionNames(enabledInstanceExtensions.data());

  mInstance = vk::createInstanceUnique(instanceCreateInfo);

#ifdef ENABLE_VK_DEBUG
  // Now we have an instance we can register the debug callback
  Util::initDidl(mInstance.get());
  Util::initDebugMessenger(mInstance.get());
#endif

  mPhysicalDevices = mInstance->enumeratePhysicalDevices();
  if( mPhysicalDevices.empty() ) throw std::runtime_error("Failed to enumerate physical devices");

  // Device order in the list isn't guaranteed, likely the integrated gpu is first
  std::sort(mPhysicalDevices.begin(), mPhysicalDevices.end(), [&](auto& a, auto& b) {
    vk::PhysicalDeviceProperties propsA;
    a.getProperties(&propsA);
    vk::PhysicalDeviceProperties propsB;
    b.getProperties(&propsB);
#ifdef PREFER_DISCRETE_GPU
    // Above all else pick dedicated over integrated
    if( propsA.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
        !(propsB.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)) {
        return true;
    } else if ( !(propsA.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) &&
                (propsB.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)) {
        return false;
    }
#endif
    // Otherwise sort by Vulkan version..There's no clear way to choose here
    if( propsA.apiVersion > propsB.apiVersion ) return true;
    return false;
  });
}

void DeviceInstance::createLogicalDevice(std::vector<vk::QueueFlags> qFlags) {

  std::vector<vk::DeviceQueueCreateInfo> queueInfo;
  auto qFamProps = mPhysicalDevices[0].getQueueFamilyProperties();
  float queuePriorities = 1.f;
  for( auto& qF : qFlags ) {
    auto it = std::find_if(qFamProps.begin(), qFamProps.end(), [&](auto& p) {
      if( p.queueFlags & qF ) return true;
      throw std::runtime_error("DeviceInstance::createLogicalDevice: Physical device doesn't support requested queue types");
      return false;
    });
    if( it == qFamProps.end() ) continue;

    auto qFamIdx = static_cast<uint32_t>(it - qFamProps.begin());

    auto it2 = std::find_if(queueInfo.begin(), queueInfo.end(), [&](auto& p) {
      return p.queueFamilyIndex == qFamIdx;
    });
    if( it2 != queueInfo.end() ) continue;

    auto qInfo = vk::DeviceQueueCreateInfo()
        .setFlags({})
        .setQueueFamilyIndex(qFamIdx)
        .setQueueCount(1)
        .setPQueuePriorities(&queuePriorities);
    queueInfo.emplace_back(qInfo);
  }
  //if( queueInfo.size() != qFlags.size() )  throw std::runtime_error("DeviceInstance::createLogicalDevice: Physical device doesn't support requested queue types");

  auto supportedExtensions = mPhysicalDevices.front().enumerateDeviceExtensionProperties();
  std::vector<const char*> enabledDeviceExtensions;
  // Allows the viewport to be flipped, core in vk 1.1
  enabledDeviceExtensions.emplace_back("VK_KHR_maintenance1");
#if defined(VK_USE_PLATFORM_WIN32_KHR) || defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_LIB_XCB_KHR) || defined(USE_GLFW)
  // If building with UI support we need the swapchain extension - allows presenting images to the window
  enabledDeviceExtensions.push_back("VK_KHR_swapchain");
  for( auto& e : enabledDeviceExtensions ) Util::ensureExtension(supportedExtensions, e);
#endif

std::vector<const char*> enabledLayers;
//#ifdef DEBUG
//  enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
//#endif
//  auto supportedLayers = mPhysicalDevices.front().enumerateDeviceLayerProperties();
//  for( auto& e : enabledLayers ) Util::ensureLayer(supportedLayers, e);

  // The features of the physical device
  auto deviceSupportedFeatures = mPhysicalDevices.front().getFeatures();

  // The features we require, we get very little without requesting these
  // As listed page 17
  // TODO: After creation the enabled features are set in this struct, will want to keep it for later
  auto deviceRequiredFeatures = vk::PhysicalDeviceFeatures()
      .setMultiDrawIndirect(deviceSupportedFeatures.multiDrawIndirect)
      .setTessellationShader(true)
      .setGeometryShader(true);

  auto info = vk::DeviceCreateInfo()
      .setFlags({})
      .setQueueCreateInfoCount(queueInfo.size())
      .setPQueueCreateInfos(queueInfo.data())
      .setEnabledLayerCount(enabledLayers.size())
      .setPpEnabledLayerNames(enabledLayers.data())
      .setEnabledExtensionCount(static_cast<uint32_t>(enabledDeviceExtensions.size()))
      .setPpEnabledExtensionNames(enabledDeviceExtensions.data())
      .setPEnabledFeatures(&deviceRequiredFeatures)
      ;

  mDevice = mPhysicalDevices.front().createDeviceUnique(info);

  for( auto i=0u; i < queueInfo.size(); ++i ) {
    auto famIdx = queueInfo[i].queueFamilyIndex;
    auto famProps = mPhysicalDevices.front().getQueueFamilyProperties();

    auto qRef = QueueRef();
    qRef.famIndex = famIdx;
    qRef.queue = mDevice->getQueue(famIdx, 0);
    qRef.flags = famProps[famIdx].queueFlags;
    mQueues.emplace_back( qRef );
  }
}

DeviceInstance::QueueRef* DeviceInstance::getQueue( vk::QueueFlags flags ) {
  auto it = std::find_if(mQueues.begin(), mQueues.end(), [&]( auto& q) {
    return flags & q.flags;
  });
  if( it == mQueues.end() ) return nullptr;
  return &(*it);
}

void DeviceInstance::waitAllDevicesIdle() {
  mDevice->waitIdle();
}


vk::UniqueCommandPool DeviceInstance::createCommandPool( vk::CommandPoolCreateFlags flags, DeviceInstance::QueueRef& queue ) {
  auto info = vk::CommandPoolCreateInfo()
      .setFlags(flags)
      .setQueueFamilyIndex(queue.famIndex);
  return mDevice->createCommandPoolUnique(info);
}

vk::UniqueBuffer DeviceInstance::createBuffer( vk::DeviceSize size, vk::BufferUsageFlags usageFlags ) {
  // Exclusive buffer of size, for usageFlags
  auto info = vk::BufferCreateInfo()
      .setFlags({})
      .setSize(size)
      .setUsage(usageFlags);
  return mDevice->createBufferUnique(info);
}

vk::UniqueImage DeviceInstance::createImage(
    vk::ImageType type, vk::Format format, vk::Extent3D extent,
    uint32_t mipLevels, uint32_t arrayLayers, vk::SampleCountFlagBits sampleCountFlags, vk::ImageTiling tiling,
    vk::ImageUsageFlags usageFlags, vk::SharingMode sharingMode, std::vector<uint32_t> queueFamilies,
    vk::ImageLayout layout) {

  auto info = vk::ImageCreateInfo()
    // .setFlags() Support for sparse images (Ones where not all the memory is allocated)
    .setImageType(type)
    .setFormat(format)
    .setExtent(extent)
    .setMipLevels(mipLevels)
    .setArrayLayers(arrayLayers)
    .setSamples(sampleCountFlags)
    .setTiling(tiling)
    .setUsage(usageFlags)
    .setSharingMode(sharingMode)
    .setQueueFamilyIndexCount(queueFamilies.size())
    .setPQueueFamilyIndices( queueFamilies.size() ? queueFamilies.data() : nullptr )
    .setInitialLayout(layout);

  return mDevice->createImageUnique(info);
}

vk::UniqueImageView DeviceInstance::createImageView(
    const vk::Image& image,
    vk::ImageViewType type,
    vk::Format format,
    vk::ImageAspectFlags aspectFlags,
    uint32_t baseMipLevel,
    uint32_t mipLevels,
    uint32_t baseArrayLayer,
    uint32_t arrayLayerCount
    ) {

  auto subResourceRange = vk::ImageSubresourceRange()
      .setAspectMask(aspectFlags)
      .setBaseMipLevel(baseMipLevel)
      .setLevelCount(mipLevels)
      .setBaseArrayLayer(baseArrayLayer)
      .setLayerCount(arrayLayerCount);

  auto info = vk::ImageViewCreateInfo()
      .setImage(image)
      .setViewType(type)
      .setFormat(format)
      .setSubresourceRange(subResourceRange);
      // .setComponents() Remapping of colour channels
      ;
  return mDevice->createImageViewUnique(info);
}

vk::Format DeviceInstance::getDepthBufferFormat() {
  std::vector<vk::Format> desiredFormats = {
    vk::Format::eD32Sfloat,
    vk::Format::eD32SfloatS8Uint,
    vk::Format::eD24UnormS8Uint
  };
  auto depthFormatFlags = vk::FormatFeatureFlagBits::eDepthStencilAttachment;

  auto it = desiredFormats.begin();
  while( it != desiredFormats.end() ) {
      // TODO: shouldn't just use the first physical device, but that's what the rest of the class does
      auto deviceFormatProps = mPhysicalDevices[0].getFormatProperties(*it);
      if( deviceFormatProps.optimalTilingFeatures & depthFormatFlags) {
          // Found a working format
          break;
      }
  }
  if( it == desiredFormats.end() ) throw std::runtime_error("Failed to select format for depth buffer");
  return *it;
}

/// Select a device memory heap based on flags (vk::MemoryRequirements::memoryTypeBits)
uint32_t DeviceInstance::selectDeviceMemoryHeap( vk::MemoryRequirements memoryRequirements, vk::MemoryPropertyFlags requiredFlags ) {
  // Initial implementation doesn't have any real requirements, just select the first compatible heap
  vk::PhysicalDeviceMemoryProperties memoryProperties = mPhysicalDevices.front().getMemoryProperties();

  auto typeFilter = memoryRequirements.memoryTypeBits;

  for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags) {
        return i;
      }
  }
  throw std::runtime_error("Failed to find suitable heap type for flags: " + vk::to_string(requiredFlags));
}

/// Allocate device memory suitable for the specified buffer
vk::UniqueDeviceMemory DeviceInstance::allocateDeviceMemoryForBuffer( vk::Buffer& buffer, vk::MemoryPropertyFlags userReqs ) {
  // Find out what kind of memory the buffer needs
  vk::MemoryRequirements memReq = mDevice->getBufferMemoryRequirements(buffer);
  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );
  auto info = vk::MemoryAllocateInfo()
      .setAllocationSize(memReq.size)
      .setMemoryTypeIndex(heapIdx);
  return mDevice->allocateMemoryUnique(info);
}


vk::UniqueDeviceMemory DeviceInstance::allocateDeviceMemoryForImage( const vk::UniqueImage& image, vk::MemoryPropertyFlags userReqs ) {
  // Find out what kind of memory the image needs
  auto memReq = mDevice->getImageMemoryRequirements(image.get());
  auto heapIdx = selectDeviceMemoryHeap(memReq, userReqs );
  auto info = vk::MemoryAllocateInfo()
      .setAllocationSize(memReq.size)
      .setMemoryTypeIndex(heapIdx);
  return mDevice->allocateMemoryUnique(info);
}

void DeviceInstance::bindMemoryToImage(const vk::UniqueImage& image, const vk::UniqueDeviceMemory& memory, vk::DeviceSize offset) {
  mDevice->bindImageMemory(image.get(), memory.get(), offset);
}

/// Bind memory to a buffer
void DeviceInstance::bindMemoryToBuffer(vk::Buffer& buffer, vk::DeviceMemory& memory, vk::DeviceSize offset) {
  mDevice->bindBufferMemory(buffer, memory, offset);
}

/// Map a region of device memory to host memory
void* DeviceInstance::mapMemory( vk::DeviceMemory& deviceMem, vk::DeviceSize offset, vk::DeviceSize size ) {
  return mDevice->mapMemory(deviceMem, offset, size);
}

/// Unmap a region of device memory
void DeviceInstance::unmapMemory( vk::DeviceMemory& deviceMem ) {
  mDevice->unmapMemory(deviceMem);
}

/// Flush memory/caches
void DeviceInstance::flushMemoryRanges( vk::ArrayProxy<const vk::MappedMemoryRange> mem ) {
  mDevice->flushMappedMemoryRanges(mem.size(), mem.data());
}

