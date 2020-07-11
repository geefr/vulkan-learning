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

#include <glm/glm.hpp>

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

  struct VertexData
  {
    float vertCoord[3] = {0.f,0.f,0.f};
    float vertColour[4] = {0.f,0.f,0.f,0.f};
  };

  // A mesh - A block of data to be rendered
  struct Mesh {
	Mesh(const std::vector<VertexData>& verts);

	std::vector<VertexData> verts;
	std::unique_ptr<SimpleBuffer> mVertexBuffer;
	// TODO: Store command buffer on a per-mesh basis, or recreate each frame? What's faster?
  };

  /**
   * Render a mesh (submit it to the render pipeline)
   * Called by any mesh nodes in the node graph during the render traversal
   */
  void renderMesh( std::shared_ptr<Mesh>, glm::mat4x4 mvp );

  void initWindow();
  void initVK();

  /**
   * Render a frame
   * @return true if the engine should continue rendering, false if we should quit
   */
  bool frame();
  void cleanup();

private:
  void createVertexBuffers();
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer);
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
