#ifndef VULKANAPP_H
#define VULKANAPP_H

#include "util/windowintegration.h"
#include "util/deviceinstance.h"
#include "util/framebuffer.h"
#include "util/simplebuffer.h"
#include "util/graphicspipeline.h"

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

class VulkanApp
{
public:
  VulkanApp(){}
  ~VulkanApp(){}

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

  // sizeof must be multiple of 4 here, no checking performed later
  struct PushConstant_test {
    float scale = 1.f;
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

  PushConstant_test mPushConstants;
  float mPushConstantsScaleFactorDelta = 0.025f;
  std::string mGeomName = "triangle";
  int scaleCount = 0;

  std::map<std::string, std::unique_ptr<SimpleBuffer>> mVertexBuffers;

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

  vk::PushConstantRange mPushConstant_test_range;
};

#endif // VULKANAPP_H
