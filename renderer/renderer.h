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

#include "vertex.h"
#include "mesh.h"
#include "material.h"

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
  
  /// Per-frame uniforms, binding = 0
  struct UBOSetPerFrame {
    glm::mat4x4 viewMatrix;
    glm::mat4x4 projectionMatrix;   
  };

  /// Per-Material/Mesh uniforms, Binding = 1
  struct UBOSetPerMaterial {
    // TODO
  };

  /// Push constants for the most frequently changing data
  /// @note Size must be multiple of 4 here, renderer doesn't check size
  /// @note Limit of 128 bytes - Minimum capacity required by spec
  struct PushConstant_matrices {
    glm::mat4x4 model;
  };

  /**
   * Create buffers/upload to GPU
   */
  std::unique_ptr<SimpleBuffer> createSimpleVertexBuffer(std::vector<Vertex> verts);
  std::unique_ptr<SimpleBuffer> createSimpleIndexBuffer(std::vector<uint32_t> indices);

  /**
   * Render a mesh (submit it to the render pipeline)
   * Called by any mesh nodes in the node graph during the render traversal
   */
  void renderMesh( std::shared_ptr<Mesh> mesh, glm::mat4x4 modelMat );

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
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer, uint32_t frameIndex);
  void createDescriptorSets();

  // Reference to the Engine, used to pass back window events/other renderer specific actions
  Engine& mEngine;

  // The window itself
  GLFWwindow* mWindow = nullptr;
  int mWindowWidth = 800;
  int mWindowHeight = 600;

  // Our classes to obfuscate the verbosity of vulkan somewhat
  // Remember deletion order matters
  std::unique_ptr<DeviceInstance> mDeviceInstance;
  std::unique_ptr<WindowIntegration> mWindowIntegration;
  std::unique_ptr<FrameBuffer> mFrameBuffer;
  std::unique_ptr<GraphicsPipeline> mGraphicsPipeline;

  DeviceInstance::QueueRef* mQueue = nullptr;

  vk::UniqueDescriptorPool mDescriptorPool;
  std::vector<vk::DescriptorSet> mDescriptorSets; // Owned by pool

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  // Members managing data on a per-frame basis
  // Vectors used here to maintain independent copies of the data
  // as we can't modify it when it's already in use for rendering
  // a frame
  uint32_t mMaxFramesInFlight = 3u;
  std::vector<vk::UniqueSemaphore> mImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> mRenderFinishedSemaphores;
  std::vector<vk::UniqueFence> mFrameInFlightFences;
  std::vector<std::unique_ptr<SimpleBuffer>> mUBOPerFrameInFlight;
  std::vector<std::unique_ptr<SimpleBuffer>> mUBOPerMaterialInFlight;

  // Push constants can be updated at any point however
  vk::PushConstantRange mPushConstant_matrices_range;

  // Members used to track data during the nodegraph traversal
  // render will happen once this is populated
  glm::mat4x4 mViewMatrix;
  glm::mat4x4 mProjectionMatrix;
  struct MeshRenderInstance {
    std::shared_ptr<Mesh> mesh;
    glm::mat4x4 modelMatrix;
  };
  std::list<MeshRenderInstance> mFrameMeshesToRender;
};

#endif
