#include "windowintegration.h"

#include <iostream>

WindowIntegration::WindowIntegration(vk::Instance& instance, vk::PhysicalDevice& pDevice, uint32_t queueFamilyIndex, vk::Device& device, GLFWwindow* window) {
  createSurfaceGLFW(instance, window);
  createSwapChain(pDevice, queueFamilyIndex, device);
  createSwapChainImageViews(device);
}


#ifdef USE_GLFW
void WindowIntegration::createSurfaceGLFW(vk::Instance& instance, GLFWwindow* window) {
  VkSurfaceKHR surface;
  if(glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("createSurfaceGLFW: Failed to create window surface");
  }
  mSurface.reset(surface);
}
#endif


void WindowIntegration::createSwapChain(vk::PhysicalDevice& pDevice, uint32_t queueFamilyIndex, vk::Device& device) {
  if( !pDevice.getSurfaceSupportKHR(queueFamilyIndex, mSurface.get()) ) throw std::runtime_error("createSwapChainXlib: Physical device doesn't support surfaces");

  // Parameters used in swapchain must comply with limits of the surface
  auto caps = pDevice.getSurfaceCapabilitiesKHR(mSurface.get());

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
  auto surfaceFormats = pDevice.getSurfaceFormatsKHR(mSurface.get());
  // TODO: Just choosing the first one here..
  mSwapChainFormat = surfaceFormats.front().format;
  auto chosenColourSpace = surfaceFormats.front().colorSpace;
  mSwapChainExtent = caps.currentExtent;
  vk::SwapchainCreateInfoKHR info(
        vk::SwapchainCreateFlagsKHR(),
        mSurface.get(),
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

  mSwapChain = device.createSwapchainKHRUnique(info);

  mSwapChainImages = device.getSwapchainImagesKHR(mSwapChain.get());
}

void WindowIntegration::createSwapChainImageViews(vk::Device& device) {
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
    mSwapChainImageViews.emplace_back( device.createImageViewUnique(info) );
  }
}

