#ifndef VULKANAPP_H
#define VULKANAPP_H

#include "util/windowintegration.h"
#include "util/deviceinstance.h"
#include "util/framebuffer.h"
#include "util/simplebuffer.h"
#include "util/graphicspipeline.h"

#include "physics.h"

#ifdef USE_GLFW
# define GLFW_INCLUDE_VULKAN
# include <GLFW/glfw3.h>
#endif

// https://github.com/KhronosGroup/Vulkan-Hpp
#include <vulkan/vulkan.hpp>

#include "glm/glm.hpp"

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

class VulkanApp
{
public:
  VulkanApp() : mPhysics(10) {}
  ~VulkanApp(){}

  void run() {
    initWindow();
    initVK();
    loop();
    cleanup();
  }

  // sizeof must be multiple of 4 here, no checking performed later
  struct PushConstants {
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
  };

private:
  void initWindow();
  void initVK();
  void createVertexBuffers();
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer);
  void loop();
  void cleanup();
  double now();

  // The window itself
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

  PushConstants mPushConstants;
  float mPushConstantsScaleFactorDelta = 0.025f;
  int scaleCount = 0;

  std::unique_ptr<SimpleBuffer> mParticleVertexBuffer;

  // Our classyboys to obfuscate the verbosity of vulkan somewhat
  // Remember deletion order matters
  std::unique_ptr<DeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<GraphicsPipeline> mGraphicsPipeline;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  uint32_t mMaxFramesInFlight = 3u;
  std::vector<vk::UniqueSemaphore> mImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> mRenderFinishedSemaphores;
  std::vector<vk::UniqueFence> mFrameInFlightFences;

  vk::PushConstantRange mPushContantsRange;

  Physics mPhysics;
  double mLastTime = 0.;
  double mCurTime = 0.;
};

#endif // VULKANAPP_H
