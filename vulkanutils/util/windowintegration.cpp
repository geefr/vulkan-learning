/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#include "windowintegration.h"

#include <iostream>

WindowIntegration::WindowIntegration(DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue)
  : mDeviceInstance(deviceInstance) {
}

#ifdef USE_GLFW
WindowIntegration::WindowIntegration(GLFWwindow* window, DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue)
  : WindowIntegration(deviceInstance, queue) {
  mGLFWWindow = window;
  createSurfaceGLFW(window);
  createSwapChain(queue);
  createSwapChainImageViews();
}
#endif

WindowIntegration::~WindowIntegration() {
  for(auto& p : mSwapChainImageViews) p.reset();
  mDepthImage.reset();
  mSwapChain.reset();
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
}
#endif


void WindowIntegration::createSwapChain(DeviceInstance::QueueRef& queue) {
  if( !mDeviceInstance.physicalDevice().getSurfaceSupportKHR(queue.famIndex, mSurface) ) throw std::runtime_error("createSwapChainXlib: Physical device doesn't support surfaces");

  // Parameters used in swapchain must comply with limits of the surface
  auto caps = mDeviceInstance.physicalDevice().getSurfaceCapabilitiesKHR(mSurface);

  // First the number of images (none, double, triple buffered)
  auto numImages = 3u; // We need 3
  if( caps.minImageCount != 0 ) numImages = std::max(numImages, caps.minImageCount);
  if( caps.maxImageCount != 0 ) numImages = std::min(numImages, caps.maxImageCount);
  if( numImages != 3u ) {
    throw std::runtime_error("Unable to create swap chain with 3 images, hardware supports between " + std::to_string(caps.minImageCount) +
    " and " + std::to_string(caps.maxImageCount));
  }

  // Ideally we want full alpha support
//  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::ePreMultiplied; // TODO: Pre or post? can't remember the difference
//  if( !(caps.supportedCompositeAlpha & alphaMode) ) {
//    std::cerr << "Surface doesn't support full alpha, falling back to opaque" << std::endl;
//    alphaMode = vk::CompositeAlphaFlagBitsKHR::eOpaque;
//  }
  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::eOpaque;

  auto imageUsage = vk::ImageUsageFlags(vk::ImageUsageFlagBits::eColorAttachment);
  if( !(caps.supportedUsageFlags & imageUsage) ) throw std::runtime_error("Surface doesn't support color attachment");

  // caps.maxImageArrayLayers;
  // caps.minImageExtent;
  // caps.currentTransform;
  // caps.supportedTransforms;

  // Choose a pixel format
  auto formats = mDeviceInstance.physicalDevice().getSurfaceFormatsKHR(mSurface);
  auto presentModes = mDeviceInstance.physicalDevice().getSurfacePresentModesKHR(mSurface);

  mSwapChainFormat = chooseSwapChainFormat(formats);
  // mSwapPresentMode = chooseSwapChainPresentMode(presentModes);
  mSwapPresentMode = vk::PresentModeKHR::eImmediate;
  mSwapChainExtent = chooseSwapChainExtent(caps);

  auto info = vk::SwapchainCreateInfoKHR()
      .setSurface(mSurface)
      .setMinImageCount(numImages)
      .setImageFormat(mSwapChainFormat.format)
      .setImageColorSpace(mSwapChainFormat.colorSpace)
      .setImageExtent(mSwapChainExtent)
      .setImageArrayLayers(1)
      .setImageUsage(imageUsage)
      .setImageSharingMode(vk::SharingMode::eExclusive) // Fine if using a single queue for graphics & present
      .setPreTransform(caps.currentTransform)
      .setCompositeAlpha(alphaMode)
      .setPresentMode(mSwapPresentMode)
      .setClipped(true)
      .setOldSwapchain({})
      ;

  mSwapChain = mDeviceInstance.device().createSwapchainKHRUnique(info);
  mSwapChainImages = mDeviceInstance.device().getSwapchainImagesKHR(mSwapChain.get());

  mDepthFormat = mDeviceInstance.getDepthBufferFormat();

  // Create depth buffer resources
  mDepthImage.reset(new SimpleImage(
                      mDeviceInstance,
                      vk::ImageType::e2D,
                      vk::ImageViewType::e2D,
                      mDepthFormat,
                      {mSwapChainExtent.width, mSwapChainExtent.height, 1},
                      1,
                      1,
                      vk::SampleCountFlagBits::e1,
                      vk::ImageUsageFlagBits::eDepthStencilAttachment,
                      vk::ImageLayout::eDepthAttachmentOptimal
  ));
}

void WindowIntegration::createSwapChainImageViews() {
  for( auto i = 0u; i < mSwapChainImages.size(); ++i ) {
    auto info = vk::ImageViewCreateInfo()
        .setFlags({})
        .setImage(mSwapChainImages[i])
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(mSwapChainFormat.format)
        .setComponents({})
        .setSubresourceRange({{vk::ImageAspectFlagBits::eColor},0, 1, 0, 1})
        ;
    mSwapChainImageViews.emplace_back( mDeviceInstance.device().createImageViewUnique(info) );
  }
}

vk::SurfaceFormatKHR WindowIntegration::chooseSwapChainFormat(std::vector<vk::SurfaceFormatKHR> formats) const {
  // Ideally we want 32-bit srgb
  for( auto& format : formats ) {
      if( format.format == vk::Format::eB8G8R8A8Srgb &&
          format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) {
          return format;
        }
  }
  // TODO: Should rank the remaining ones on preference
  // for now just use the first option
  return formats.front();
}

vk::PresentModeKHR WindowIntegration::chooseSwapChainPresentMode(std::vector<vk::PresentModeKHR> presentModes) const {
  // Present mode is really important - The conditions for showing image on the screen.
  // - immediate - No delay, may cause tearing
  // - fifo - vsync/at at vblank, double buffered
  // - fifo_relaxed - lazy vsync, may cause tearing but reduce stutters?
  // - mailbox - vsync, triple buffered
  for( auto& mode : presentModes ) {
      if( mode == vk::PresentModeKHR::eMailbox ) return mode;
  }
  // Guaranteed to be supported
  return vk::PresentModeKHR::eFifo;
}

vk::Extent2D WindowIntegration::chooseSwapChainExtent(vk::SurfaceCapabilitiesKHR caps) const {
  // The size of the swapchain images
  // uint max is a special value from some window managers, means we're allowed to have a different
  // dimensions here than the actual window.
  if( caps.currentExtent.width != std::numeric_limits<uint32_t>::max() ) {
      return caps.currentExtent;
  } else {
#ifdef USE_GLFW
    int width, height;
    glfwGetFramebufferSize(mGLFWWindow, &width, &height);
    return {
      std::max( caps.minImageExtent.width, std::min(caps.maxImageExtent.width, static_cast<uint32_t>(width))),
      std::max( caps.minImageExtent.height, std::min(caps.maxImageExtent.height, static_cast<uint32_t>(height)))
    };
#else
#error "only GLFW Window Integration is available right now"
#endif
  }
}
