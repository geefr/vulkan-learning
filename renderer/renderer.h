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
#include "light.h"

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
// Shader interface types/declarations below here
private:
  // Global constants for the renderer/pipeline
  // Defaults for these should be more than ridiculous,
  // so shouldn't need to touch these.
  static const uint32_t MAX_LIGHTS = 100;
  struct GraphicsSpecConstants {
    uint32_t maxLights = MAX_LIGHTS;
  };
  GraphicsSpecConstants mGraphicsSpecConstants;
  // As defined by glTF Punctual lights extension
  // https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
  struct ShaderLightData {
    glm::vec4 posOrDir; // position if w == 1 else direction
    glm::vec4 colour;   // w == intensity
    glm::vec4 typeAndParams; // x == range, y == innerConeCos, z == outerConeCos, w == Light::Type
  };
  /// Per-frame uniforms, binding = 0
  struct UBOSetPerFrame {
    glm::mat4x4 viewMatrix;
    glm::mat4x4 projectionMatrix;
    glm::vec4 eyePos;
    float numLights;
    float pad1;
    float pad2;
    float pad3;
    ShaderLightData lights[MAX_LIGHTS];
  };

  /// Per-Material/Mesh uniforms, Binding = 1
  struct UBOSetPerMaterial {
    glm::vec4 baseColourFactor;
    glm::vec4 emissiveFactor;
    glm::vec4 diffuseFactor;
    glm::vec3 specularFactor;
    float pad;
    float alphaCutOff;
    // TODO: And magically we're 16-byte aligned
    // When adding more data be careful here
  };

  /// Push constants for the most frequently changing data
  /// @note Size must be multiple of 4 here, renderer doesn't check size
  /// @note Limit of 128 bytes - Minimum capacity required by spec
  struct PushConstantSet {
    glm::mat4x4 modelMatrix;
  };

// The renderer class itself
public:
  Renderer( Engine& engine );
  ~Renderer();

  /**
   * Create buffers/upload to GPU
   */
  std::unique_ptr<SimpleBuffer> createSimpleVertexBuffer(std::vector<Vertex> verts);
  std::unique_ptr<SimpleBuffer> createSimpleIndexBuffer(std::vector<uint32_t> indices);

  /**
   * Called by any mesh nodes in the node graph during the render traversal
   * Logs the mesh for submission as part of the frame
   */
  void renderMesh( std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, glm::mat4x4 modelMat );
  /// Called during node graph traversal
  /// Logs a light for the frame
  void renderLight( const Light& l );

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
  
  /// Initialise Descriptor pool, layouts for the renderer - Per-frame constants
  void initDescriptorSetsForRenderer();
  void createDescriptorSetsForRenderer();

  /// Initialise descriptor pool, layouts for mesh data - Per-mesh constants (materials)
  void initDescriptorSetsForMeshes();
  void createDefaultDescriptorSetForMesh();
  void createDescriptorSetForMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material);

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

  // Descriptor pool for the renderer's per-frame data
  vk::UniqueDescriptorPool mDescriptorPoolRenderer;
  std::vector<vk::DescriptorSet> mDescriptorSetsRenderer; // Owned by pool

  // Descriptor pool for per-mesh data (materials)
  vk::UniqueDescriptorPool mDescriptorPoolMeshes;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  // Members managing data on a per-frame basis
  // Vectors used here to maintain independent copies of the data
  // as we can't modify it when it's already in use for rendering
  // a frame
  uint32_t mMaxFramesInFlight = 3u;
  uint32_t mFrameIndex = 0u;
  std::vector<vk::UniqueSemaphore> mImageAvailableSemaphores;
  std::vector<vk::UniqueSemaphore> mRenderFinishedSemaphores;
  // A fence for each of the in-flight frames
  std::vector<vk::UniqueFence> mInFlightFences;
  // Tracking of fences against swapchain images
  // The fence for an image in the chain will be set to one of the
  // in flight fences when frame is submitted.
  std::vector<vk::Fence> mImagesInFlightFences;
  std::vector<std::unique_ptr<SimpleBuffer>> mUBOPerFrameInFlight;

  // Push constants can be updated at any point however
  vk::PushConstantRange mPushConstantSetRange;

  // Members used to track data during the nodegraph traversal
  // render will happen once this is populated
  glm::mat4x4 mViewMatrix = glm::mat4x4(1.0f);
  glm::mat4x4 mProjectionMatrix = glm::mat4x4(1.0f);
  glm::vec3 mEyePos = {0.f,0.f,0.f};

  // An instruction to the renderer to draw the mesh
  struct MeshRenderInstance {
    std::shared_ptr<Mesh> mesh;
    glm::mat4x4 modelMatrix;
  };
  std::list<MeshRenderInstance> mFrameMeshesToRender;

  // Shader resources for each rendered mesh
  struct MeshRenderData {
    vk::DescriptorSet descriptorSet;
    std::unique_ptr<SimpleBuffer> uboMaterial;
  };
  std::map<std::shared_ptr<Mesh>, MeshRenderData> mMeshRenderData;

  std::unique_ptr<SimpleBuffer> mUBOMeshDataDefault;
  vk::DescriptorSet mDescriptorSetMeshDataDefault;

  // The lights to render in the frame
  std::vector<ShaderLightData> mLightsToRender;
};

#endif
