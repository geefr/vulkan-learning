#ifndef VULKANAPP_H
#define VULKANAPP_H

#include "util/windowintegration.h"
#include "util/deviceinstance.h"
#include "util/framebuffer.h"
#include "util/simplebuffer.h"
#include "util/graphicspipeline.h"
#include "util/computepipeline.h"

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
  // TODO: Must be a multiple of 4, we don't validate buffer size before throwing at vulkan
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
    glm::mat4 modelMatrix;
    glm::mat4 viewMatrix;
    glm::mat4 projMatrix;
  };

private:
  void initWindow();
  void initVK();
  void createVertexBuffers();
  void createComputeBuffers();
  void createComputeDescriptorSet();
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer, vk::Buffer& particleVertexBuffer);
  void buildComputeCommandBuffer(vk::CommandBuffer& commandBuffer, bool uploadParticles);
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

  std::vector<std::unique_ptr<SimpleBuffer>> mParticleVertexBuffers;


  // TODO: Hardcoded buffer sizes
  uint32_t mComputeBufferWidth = 10;
  uint32_t mComputeBufferHeight = 1;
  uint32_t mComputeBufferDepth = 1;
  uint32_t mComputeGroupSizeX = 1;
  uint32_t mComputeGroupSizeY = 1;
  uint32_t mComputeGroupSizeZ = 1;
  std::vector<std::unique_ptr<SimpleBuffer>> mComputeDataBuffers;
  vk::UniqueDescriptorPool mComputeDescriptorPool;
  vk::DescriptorSet mComputeDescriptorSet; // Owned by pool
  vk::UniqueCommandPool mComputeCommandPool;
  std::vector<vk::UniqueCommandBuffer> mComputeCommandBuffers;

  // Our classyboys to obfuscate the verbosity of vulkan somewhat
  // Remember deletion order matters
  std::unique_ptr<DeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<GraphicsPipeline> mGraphicsPipeline;

  std::unique_ptr<ComputePipeline> mComputePipeline;

  DeviceInstance::QueueRef* mGraphicsQueue = nullptr;
  DeviceInstance::QueueRef* mComputeQueue = nullptr;

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
