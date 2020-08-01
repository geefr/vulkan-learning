/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#include "windowintegration.h"
#include "util.h"

#include <iostream>

WindowIntegration::WindowIntegration(DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue, vk::SampleCountFlagBits desiredSamples)
  : mDeviceInstance(deviceInstance)
{
  mSamples = Util::maxUseableSamples(deviceInstance.physicalDevice(), desiredSamples);
}

#ifdef USE_GLFW
WindowIntegration::WindowIntegration(GLFWwindow* window, DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue, vk::SampleCountFlagBits desiredSamples)
  : WindowIntegration(deviceInstance, queue, desiredSamples) {
  mGLFWWindow = window;
  createSurfaceGLFW(window);
  createSwapChain(queue);
  createSwapChainImageViews();
  createDepthResources();
  createMultiSampleResources();
}
#endif

WindowIntegration::~WindowIntegration() {
  for(auto& p : mSwapChainImageViews) p.reset();
  mMultiSampleImage.reset();
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
  auto numImages = 3u; // We want 3
  if( caps.minImageCount != 0 ) numImages = std::max(numImages, caps.minImageCount);
  if( caps.maxImageCount != 0 ) numImages = std::min(numImages, caps.maxImageCount);

  auto alphaMode = vk::CompositeAlphaFlagBitsKHR::eOpaque;

  auto imageUsage = vk::ImageUsageFlags(vk::ImageUsageFlagBits::eColorAttachment);
  if( !(caps.supportedUsageFlags & imageUsage) ) throw std::runtime_error("Surface doesn't support color attachment");

  // Choose a pixel format
  auto formats = mDeviceInstance.physicalDevice().getSurfaceFormatsKHR(mSurface);
  auto presentModes = mDeviceInstance.physicalDevice().getSurfacePresentModesKHR(mSurface);

  mSwapChainFormat = chooseSwapChainFormat(formats);
  mSwapPresentMode = chooseSwapChainPresentMode(presentModes);
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
}

void WindowIntegration::createDepthResources() {
  auto depthFormat = mDeviceInstance.getDepthBufferFormat();

  // Create depth buffer resources
  // Depth image must be 2D, same size as colour buffers, sensible format, and device local
  mDepthImage.reset(new SimpleImage(
                      mDeviceInstance,
                      vk::ImageType::e2D,
                      vk::ImageViewType::e2D,
                      depthFormat,
                      {mSwapChainExtent.width, mSwapChainExtent.height, 1},
                      1,
                      1,
                      mSamples,
                      vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
                      vk::MemoryPropertyFlagBits::eDeviceLocal,
                      vk::ImageAspectFlagBits::eDepth
                      ));
}

void WindowIntegration::createMultiSampleResources() {
  mMultiSampleImage.reset(new SimpleImage(
                            mDeviceInstance,
                            vk::ImageType::e2D,
                            vk::ImageViewType::e2D,
                            mSwapChainFormat.format,
                            {mSwapChainExtent.width, mSwapChainExtent.height, 1},
                            1, 1,
                            mSamples,
                            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
                            vk::MemoryPropertyFlagBits::eDeviceLocal,
                            vk::ImageAspectFlagBits::eColor
                            ));
}

void WindowIntegration::createSwapChainImageViews() {
  for( auto i = 0u; i < mSwapChainImages.size(); ++i ) {

    mSwapChainImageViews.emplace_back(
          mDeviceInstance.createImageView(
            mSwapChainImages[i],
            vk::ImageViewType::e2D,
            mSwapChainFormat.format,
            vk::ImageAspectFlagBits::eColor));
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
