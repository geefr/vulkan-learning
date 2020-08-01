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
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <atomic>

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

  int windowWidth() const;
  int windowHeight() const;

private:
  void onGLFWKeyEvent(int key, int scancode, int action, int mods);
  void onGLFWFramebufferSize(int width, int height);

  // Create the swap chain, graphics pipeline, framebuffers, etc
  // Before calling make sure mQueue == The graphics/presentation queue
  void createSwapChainAndGraphicsPipeline();
  void reCreateSwapChainAndGraphicsPipeline();

  // Build command buffer(s) for the current frame
  // Will read from mPerFrameData and mPerImageData
  void buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer);
  
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
  // TODO: Currently hardcoded, should not be
  // TODO: This should be moved to WindowIntegration, the renderer shouldn't need to care
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

  // Descriptor pool for per-mesh data (materials)
  vk::UniqueDescriptorPool mDescriptorPoolMeshes;

  vk::UniqueCommandPool mCommandPool;
  std::vector<vk::UniqueCommandBuffer> mCommandBuffers;

  // Data for each of the frames-in-flight
  // Vectors used here to maintain independent copies of the data
  // as we can't modify it when it's already in use for rendering
  // a frame
  struct PerFrameData {
    vk::UniqueSemaphore imageAvailableSem;
    vk::UniqueSemaphore renderFinishedSem;
    vk::UniqueFence     renderFinishedFence;
  };

  // Data for each of the swapchains images
  struct PerImageData {
    std::unique_ptr<SimpleBuffer> ubo; // Matrices, global frame data
    vk::DescriptorSet uboDescriptor = {}; // Owned by pool
    vk::Fence fence = {}; // A fence, assigned from mFramesInFlight
  };

  uint32_t mMaxFramesInFlight = 2u;

  std::vector<PerFrameData> mPerFrameData;
  std::vector<PerImageData> mPerImageData;

  // Push constants can be updated at any point however
  vk::PushConstantRange mPushConstantSetRange;

  // Members used to track data during the nodegraph traversal
  // render will happen once this is populated
  // An instruction to the renderer to draw the mesh
  struct MeshRenderInstance {
    std::shared_ptr<Mesh> mesh;
    glm::mat4x4 modelMatrix;
  };

  struct CurrentFrameData {
    // Global frame data
    glm::mat4x4 viewMatrix = glm::mat4x4(1.0f);
    glm::mat4x4 projectionMatrix = glm::mat4x4(1.0f);
    glm::vec3 eyePos = {0.f,0.f,0.f};

    // The meshes and lights to render in the frame
    std::list<MeshRenderInstance> meshesToRender;
    std::vector<ShaderLightData> lightsToRender;

    // Tracking of which frame we're on, and which image the frame is rendering to
    uint32_t frameIndex = 0u;
    uint32_t imageIndex = 0u;
  } mCurrentFrameData;

  // Shader/Material resources for each rendered mesh
  // TODO: Will be shared between frames, assumed to never be cleared
  struct MeshRenderData {
    vk::DescriptorSet descriptorSet;
    std::unique_ptr<SimpleBuffer> uboMaterial;
  };
  std::map<std::shared_ptr<Mesh>, MeshRenderData> mMeshRenderData;

  // Default/placeholder material
  std::unique_ptr<SimpleBuffer> mUBOMeshDataDefault;
  vk::DescriptorSet mDescriptorSetMeshDataDefault;

  // Flag set by on GLFWFramebufferSize
  // Checked during rendering to trigger swapchain recreation
  std::atomic<bool> mRecreateSwapChainSoon = false;
};

#endif
