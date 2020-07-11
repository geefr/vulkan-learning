/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "util/windowintegration.h"
#include "util/deviceinstance.h"
#include "util/framebuffer.h"
#include "util/simplebuffer.h"
#include "util/pipelines/graphicspipeline.h"

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#endif

// https://github.com/KhronosGroup/Vulkan-Hpp
# include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <map>
#include <string>

class FrameBuffer;
class SimpleBuffer;
class GraphicsPipeline;
class DeviceInstance;
class WindowIntegration;

class Renderer
{
public:
  Renderer(){}
  ~Renderer(){}

  void run() {
    initWindow();
    initVK();
    loop();
    cleanup();
  }

  struct VertexData
  {
    float vertCoord[3] = {0.f,0.f,0.f};
    float vertColour[4] = {0.f,0.f,0.f,0.f};
  };

private:
  void initWindow();
  void initVK();
  void createVertexBuffers();
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer);
  void loop();
  void cleanup();

  // The window itself
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

  std::string mGeomName = "triangle";

  std::map<std::string, std::unique_ptr<SimpleBuffer>> mVertexBuffers;

  // Our classyboys to obfuscate the verbosity of vulkan somewhat
  // Remember deletion order matters
  std::unique_ptr<DeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<GraphicsPipeline> mGraphicsPipeline;

  DeviceInstance::QueueRef* mQueue = nullptr;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  uint32_t mMaxFramesInFlight = 3u;
  std::vector<vk::UniqueSemaphore> mImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> mRenderFinishedSemaphores;
  std::vector<vk::UniqueFence> mFrameInFlightFences;
};

#endif
