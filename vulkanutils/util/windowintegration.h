/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#ifndef WINDOWINTEGRATION_H
#define WINDOWINTEGRATION_H

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#else
# error "WSI type not supported by WindowIntegration"
#endif
#include <vulkan/vulkan.hpp>

#include "deviceinstance.h"
#include "simpleimage.h"

/**
 * Functionality for window system integration, swapchains and such
 */
class WindowIntegration
{
public:
  WindowIntegration() = delete;
  /// Setup the swapchain & resources
  /// @param desiredSamples The maximum multi-sampling to use. If the device can't support this the highest level up to this will be selected
  WindowIntegration(DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue, vk::SampleCountFlagBits desiredSamples);
  WindowIntegration(const WindowIntegration&) = delete;
  WindowIntegration(WindowIntegration&&) = default;
  ~WindowIntegration();

#ifdef USE_GLFW
  WindowIntegration(GLFWwindow* window, DeviceInstance& deviceInstance, DeviceInstance::QueueRef& queue, vk::SampleCountFlagBits desiredSamples);
#endif

  vk::Extent2D swapChainExtent() const { return mSwapChainExtent; }
  vk::Format swapChainFormat() const { return mSwapChainFormat.format; }
  size_t swapChainSize() const { return mSwapChainImages.size(); }
  const vk::SwapchainKHR& swapChain() const { return mSwapChain.get(); }
  const std::vector<vk::Image>& swapChainImages() const { return mSwapChainImages; }
  const std::vector<vk::UniqueImageView>& swapChainImageViews() const { return mSwapChainImageViews; }

  const vk::ImageView& sampleImageView() const { return mMultiSampleImage->view(); }
  const vk::SampleCountFlagBits& samples() const { return mSamples; }
  vk::Format sampleFormat() const { return mMultiSampleImage->format(); }

  const vk::ImageView& depthImageView() const { return mDepthImage->view(); }
  vk::Format depthFormat() const { return mDepthImage->format(); }

private:
#ifdef USE_GLFW
  void createSurfaceGLFW(GLFWwindow* window);
  GLFWwindow* mGLFWWindow = nullptr;
#endif

  void createSwapChain(DeviceInstance::QueueRef& queue);
  void createSwapChainImageViews();
  void createDepthResources();
  void createMultiSampleResources();

  vk::SurfaceFormatKHR chooseSwapChainFormat(std::vector<vk::SurfaceFormatKHR> formats) const;
  vk::PresentModeKHR chooseSwapChainPresentMode(std::vector<vk::PresentModeKHR> presentModes) const;
  vk::Extent2D chooseSwapChainExtent(vk::SurfaceCapabilitiesKHR caps) const;

  DeviceInstance& mDeviceInstance;

  // These members 'owned' by the unique pointers below
  // The swapchain - Images to render to, provided by the windowing system
  vk::SurfaceFormatKHR mSwapChainFormat;
  vk::Extent2D mSwapChainExtent;
  vk::PresentModeKHR mSwapPresentMode;
  std::vector<vk::Image> mSwapChainImages;
  // The depth buffer
  std::unique_ptr<SimpleImage> mDepthImage;
  // The render target for multisampling
  std::unique_ptr<SimpleImage> mMultiSampleImage;

  // TODO: >.< For some reason trying to use a UniqueSurfaceKHR here
  // messes up, and we end up with the wrong value for mSurface.m_owner
  // This causes a segfault when destroying the surface as it's calling
  // a function on an invalid surface
  // Maybe it's best to just use the raw types here, the smart pointers
  // are proving to be tricky and as we're still having to manually reset
  // them there's not much point using them in the first place
  vk::SurfaceKHR mSurface;

  // Order of members matters for Unique pointers
  // will be destructed in reverse order
  vk::UniqueSwapchainKHR mSwapChain;
  std::vector<vk::UniqueImageView> mSwapChainImageViews;

  vk::SampleCountFlagBits mSamples = vk::SampleCountFlagBits::e1;
};

#endif // WINDOWINTEGRATION_H
