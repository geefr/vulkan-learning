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
#include <list>
#include <map>
#include <string>

class FrameBuffer;
class SimpleBuffer;
class GraphicsPipeline;
class DeviceInstance;
class WindowIntegration;

class Engine;

class Renderer
{
public:
  Renderer( Engine& engine );
  ~Renderer();

  struct VertexData
  {
    glm::vec4 vertCoord =  {0.f,0.f,0.f,0.f};
    glm::vec4 vertColour = {1.f,1.f,1.f,1.f};
  };

  // A mesh - A block of data to be rendered
  struct Mesh {
	Mesh(const std::vector<VertexData>& v);

	/// Create buffers and upload data to the GPU
	void upload(Renderer& rend);
	/// Delete buffers
	void cleanup(Renderer& rend);

	std::vector<VertexData> verts;
	std::unique_ptr<SimpleBuffer> mVertexBuffer;
	// TODO: Store command buffer on a per-mesh basis, or recreate each frame? What's faster?
	// TODO: Currently making a separate buffer for each mesh, should have a batched version/auto-batch things
  };
  
  /// Push constants for matrix data
  /// @note Size must be multiple of 4 here, renderer doesn't check size
  struct PushConstant_matrices {
    glm::mat4x4 model;
    glm::mat4x4 view;
    glm::mat4x4 projection;
  };

  /**
   * Create a vertex buffer and upload data to the GPU
   */
  std::unique_ptr<SimpleBuffer> createSimpleVertexBuffer(std::vector<VertexData> verts);

  /**
   * Render a mesh (submit it to the render pipeline)
   * Called by any mesh nodes in the node graph during the render traversal
   */
  void renderMesh( std::shared_ptr<Mesh> mesh, glm::mat4x4 modelMat, glm::mat4x4 viewMat, glm::mat4x4 projMat );

  void initWindow();
  void initVK();

  /**
   * Poll events from the window
   * @return false to quit, true otherwise
   */
  bool pollWindowEvents();

  /**
   * Render a frame
   */
  void frameStart();
  void frameEnd();
  void waitIdle();
  void cleanup();

  void onGLFWKeyEvent(int key, int scancode, int action, int mods);

private:
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer);

  // Reference to the Engine, used to pass back window events/other renderer specific actions
  Engine& mEngine;

  // The window itself
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

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

  vk::PushConstantRange mPushConstant_matrices_range;

  struct MeshRenderInstance { 
	  std::shared_ptr<Mesh> mesh;
      glm::mat4x4 modelMatrix;
      glm::mat4x4 viewMatrix;
      glm::mat4x4 projectionMatrix;
  };

  // Members used to track data during a frame/nodegraph traversal
  std::list<MeshRenderInstance> mFrameMeshesToRender;
};

#endif
