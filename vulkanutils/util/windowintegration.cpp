#include "windowintegration.h"
#include "deviceinstance.h"

#include <iostream>

WindowIntegration::WindowIntegration(DeviceInstance& deviceInstance)
  : mDeviceInstance(deviceInstance) {
}

WindowIntegration::WindowIntegration(DeviceInstance& deviceInstance, GLFWwindow* window)
  : WindowIntegration(deviceInstance) {
  createSurfaceGLFW(window);
  createSwapChain();
  createSwapChainImageViews();
}

WindowIntegration::~WindowIntegration() {
  for(auto& p : mSwapChainImageViews) p.reset();
  mSwapChain.reset();
  //mSurface.reset();
  vkDestroySurfaceKHR(mDeviceInstance.instance(), mSurface, nullptr);
}

#ifdef USE_GLFW
void WindowIntegration::createSurfaceGLFW(GLFWwindow* window) {
  VkSurfaceKHR surface;
  auto& instance = mDeviceInstance.instance();
  if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("createSurfaceGLFW: Failed to create window surface");
  }
  mSurface = surface;
  //mSurface.reset(surface);
}
#endif


void WindowIntegration::createSwapChain() {
  if( !mDeviceInstance.physicalDevice().getSurfaceSupportKHR(mDeviceInstance.queueFamilyIndex(), mSurface) ) throw std::runtime_error("createSwapChainXlib: Physical device doesn't support surfaces");

  // Parameters used in swapchain must comply with limits of the surface
  auto caps = mDeviceInstance.physicalDevice().getSurfaceCapabilitiesKHR(mSurface);

  // First the number of images (none, double, triple buffered)
  auto numImages = 3u;
  while( numImages > caps.maxImageCount ) numImages--;
  if( numImages != 3u ) std::cerr << "Creating swapchain with " << numImages << " images" << std::endl;
  if( numImages < caps.minImageCount ) throw std::runtime_error("Unable to create swapchain, invalid num images");

  // Ideally we want full alpha support
  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied; // TODO: Pre or post? can't remember the difference
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
  auto surfaceFormats = mDeviceInstance.physicalDevice().getSurfaceFormatsKHR(mSurface);
  // TODO: Just choosing the first one here..
  mSwapChainFormat = surfaceFormats.front().format;
  auto chosenColourSpace = surfaceFormats.front().colorSpace;
  mSwapChainExtent = caps.currentExtent;
  vk::SwapchainCreateInfoKHR info(
        vk::SwapchainCreateFlagsKHR(),
        mSurface,
        numImages, // Num images in the swapchain
        mSwapChainFormat, // Pixel format
        chosenColourSpace, // Colour space
        mSwapChainExtent, // Size of window
        1, // Image array layers
        imageUsage, // Image usage flags
        vk::SharingMode::eExclusive, 0, nullptr, // Exclusive use by one queue
        caps.currentTransform, // flip/rotate before presentation
        alphaMode, // How alpha is mapped
        vk::PresentModeKHR::eFifo, // vsync (mailbox is vsync or faster, fifo is vsync)
        true, // Clipped - For cases where part of window doesn't need to be rendered/is offscreen
        vk::SwapchainKHR() // The old swapchain to delete
        );

  mSwapChain = mDeviceInstance.device().createSwapchainKHRUnique(info);
  mSwapChainImages = mDeviceInstance.device().getSwapchainImagesKHR(mSwapChain.get());
}

void WindowIntegration::createSwapChainImageViews() {
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
    mSwapChainImageViews.emplace_back( mDeviceInstance.device().createImageViewUnique(info) );
  }
}

