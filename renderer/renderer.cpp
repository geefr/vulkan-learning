#include "renderer.h"
#include "engine.h"

#include <mutex>
#include <functional>

using namespace std::placeholders;

Renderer::Renderer(Engine& engine)
  : mEngine(engine) {}

Renderer::~Renderer() {
  cleanup();
}

void Renderer::initWindow() {
  // TODO: This should be in a separate class - Renderer shouldn't be tied to one window system
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan Experiment", nullptr, nullptr);

  // Initialise event handlers
  glfwSetWindowUserPointer(mWindow, this);
  glfwSetKeyCallback(mWindow, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
    static_cast<Renderer*>(glfwGetWindowUserPointer(win))->onGLFWKeyEvent(key, scancode, action, mods);
  });
  glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow* win, int width, int height) {
    static_cast<Renderer*>(glfwGetWindowUserPointer(win))->onGLFWFramebufferSize(width, height);
  });
}

int Renderer::windowWidth() const { return mWindowWidth; }
int Renderer::windowHeight() const { return mWindowHeight; }

void Renderer::onGLFWKeyEvent(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    mEngine.addEvent(new KeyPressEvent(key));
  }
  else if (action == GLFW_RELEASE) {
    mEngine.addEvent(new KeyReleaseEvent(key));
  }
}

void Renderer::onGLFWFramebufferSize(int width, int height) {
  mRecreateSwapChainSoon = true;
  mWindowWidth = width;
  mWindowHeight = height;
}

void Renderer::initVK() {
  // Initialise the vulkan instance
  // GLFW can give us what extensions it requires, nice
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char*> requiredExtensions;
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) requiredExtensions.push_back(glfwExtensions[i]);

  // Just the one queue here for rendering
  std::vector<vk::QueueFlags> requiredQueues = { vk::QueueFlagBits::eGraphics };
  mDeviceInstance.reset(new DeviceInstance(requiredExtensions, {}, "Geefr Vulkan Renderer", 1, VK_API_VERSION_1_1, requiredQueues));
  mQueue = mDeviceInstance->getQueue(requiredQueues[0]);
  if (!mQueue) throw std::runtime_error("Failed to get graphics queue from device");

  // Find out what queues are available
  //auto queueFamilyProps = dev.getQueueFamilyProperties();
  //printQueueFamilyProperties(queueFamilyProps);

  createSwapChainAndGraphicsPipeline();

  // Setup per-image primitives
  // This data is assigned one for each swapchain image (which may be different to mMaxFramesInFlight)
  mPerImageData.resize(mWindowIntegration->swapChainSize());
  // Create descriptor pools
  initDescriptorSetsForRenderer();
  initDescriptorSetsForMeshes();
  // Create UBOs & descriptors for per-image data and any associated defaults
  createDescriptorSetsForRenderer();
  createDefaultDescriptorSetForMesh();

  // Setup our sync primitives
  // imageAvailable - gpu: Used to stall the pipeline until the presentation has finished reading from the image
  // renderFinished - gpu: Used to stall presentation until the command buffers have finished
  // frameInFlightFence - cpu: Used to ensure we don't render a frame twice at once, limit to mMaxFramesInFlight
  for (auto i = 0u; i < mMaxFramesInFlight; ++i) {
    PerFrameData data = {
      .imageAvailableSem = mDeviceInstance->device().createSemaphoreUnique({}),
      .renderFinishedSem = mDeviceInstance->device().createSemaphoreUnique({}),
      .renderFinishedFence = mDeviceInstance->device().createFenceUnique({ vk::FenceCreateFlagBits::eSignaled})
    };
    mPerFrameData.emplace_back(std::move(data));
  }
}

void Renderer::createSwapChainAndGraphicsPipeline() {
  // Create a logical device to interact with
  // To do this we also need to specify how many queues from which families we want to create
  // In this case just 1 queue from the first family which supports graphics
  mWindowIntegration.reset(new WindowIntegration(mWindow, *mDeviceInstance.get(), *mQueue));

  // Create the pipeline, with a flag to invert the viewport height (Switch to left handed coordinate system)
  // If changing this check the compile flags for GLM_FORCE_LEFT_HANDED - The rest of the engine uses one cs
  // and the renderer should handle it
  mGraphicsPipeline.reset(new GraphicsPipeline(*mWindowIntegration.get(), *mDeviceInstance.get(), true));

  // Build the graphics pipeline
  // In this case we can throw away the shader modules after building as they're only used by the one pipeline
  {
    mGraphicsPipeline->shaders()[vk::ShaderStageFlagBits::eVertex] = mGraphicsPipeline->createShaderModule(std::string(RENDERER_SHADER_ROOT) + "/mesh.vert.spv");
    // mGraphicsPipeline->shaders()[vk::ShaderStageFlagBits::eFragment] = mGraphicsPipeline->createShaderModule(std::string(RENDERER_SHADER_ROOT) + "/flatshading.frag.spv");
    mGraphicsPipeline->shaders()[vk::ShaderStageFlagBits::eFragment] = mGraphicsPipeline->createShaderModule(std::string(RENDERER_SHADER_ROOT) + "/phongish.frag.spv");

    // The layout of our vertex buffers
    auto vertBufferBinding = vk::VertexInputBindingDescription()
      .setBinding(0)
      .setStride(sizeof(Vertex))
      .setInputRate(vk::VertexInputRate::eVertex);
    mGraphicsPipeline->vertexInputBindings().emplace_back(vertBufferBinding);

    // Location, Binding, Format, Offset
    mGraphicsPipeline->vertexInputAttributes().emplace_back(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position));
    mGraphicsPipeline->vertexInputAttributes().emplace_back(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal));
    mGraphicsPipeline->vertexInputAttributes().emplace_back(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, uv0));
    mGraphicsPipeline->vertexInputAttributes().emplace_back(3, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, uv0));

    // Register the Descriptor set layouts on the pipeline
    mGraphicsPipeline->addDescriptorSetLayoutBinding(0, 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics);
    mGraphicsPipeline->addDescriptorSetLayoutBinding(1, 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics);

    // Register the push constant blocks
    mPushConstantSetRange = vk::PushConstantRange()
      .setStageFlags(vk::ShaderStageFlagBits::eVertex)
      .setOffset(0)
      .setSize(sizeof(PushConstantSet));
    mGraphicsPipeline->pushConstants().emplace_back(mPushConstantSetRange);

    // Setup specialisation constants
    vk::SpecializationMapEntry specs[] = {
      {0, offsetof(GraphicsSpecConstants, maxLights), sizeof(uint32_t)},
    };
    mGraphicsPipeline->specialisationConstants()[vk::ShaderStageFlagBits::eAllGraphics] = vk::SpecializationInfo(1, specs, sizeof(GraphicsSpecConstants), &mGraphicsPipeline);

    // Finally build the pipeline
    mGraphicsPipeline->build();
  }

  mFrameBuffer.reset(new FrameBuffer(mDeviceInstance->device(), *mWindowIntegration.get(), mGraphicsPipeline->renderPass()));

  // Create our command pool (So we can make command buffers
  // eResetCommandBuffer - Buffers can be reset/re-used individually, instead of needing to reset the whole pool
  mCommandPool = mDeviceInstance->createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer }, *mQueue);
  // Now make a command buffer for each swapchain image
  auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
    .setCommandPool(mCommandPool.get())
    .setCommandBufferCount(static_cast<uint32_t>(mWindowIntegration->swapChainSize()))
    .setLevel(vk::CommandBufferLevel::ePrimary);
  mCommandBuffers = mDeviceInstance->device().allocateCommandBuffersUnique(commandBufferAllocateInfo);
}

void Renderer::reCreateSwapChainAndGraphicsPipeline() {
  // Perform a partial teardown and recreate (to hand window resize/similar)
  // This method should clean up anything that's created in createSwapChainAndGraphicsPipeline
  // TODO: It's possible/more efficient to create a new swapchain while we're still rendering
  // For now just halt everything and recreate from scratch
  mDeviceInstance->waitAllDevicesIdle();

//  mPerImageData.clear();

  mCommandBuffers.clear();
  // TODO: Don't actually need to recreate the pool
  mCommandPool.reset();

  mGraphicsPipeline.reset();
  mFrameBuffer.reset();
  mWindowIntegration.reset();

  createSwapChainAndGraphicsPipeline();
}

void Renderer::buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer) {
  const auto& imageData = mPerImageData[mCurrentFrameData.imageIndex];

  auto beginInfo = vk::CommandBufferBeginInfo()
    .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution
    .setPInheritanceInfo(nullptr)
    ;
  commandBuffer.begin(beginInfo);

  // Start the render pass
  // Clear colour/depth buffers at the start
  std::array<vk::ClearValue, 2> clearVals;
  std::array<float, 4> clearColour = { 0.0f,0.0f,0.0f,1.0f };
  clearVals[0].color = vk::ClearColorValue(clearColour);
  clearVals[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

  auto renderPassInfo = vk::RenderPassBeginInfo()
    .setRenderPass(mGraphicsPipeline->renderPass())
    .setFramebuffer(frameBuffer)
    .setClearValueCount(clearVals.size())
    .setPClearValues(clearVals.data());
  renderPassInfo.renderArea.offset = vk::Offset2D(0, 0);
  renderPassInfo.renderArea.extent = mWindowIntegration->extent();

  // render commands will be embedded in primary buffer and no secondary command buffers
  // will be executed
  commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  // Bind the graphics pipeline
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline->pipeline());

  // Update the per-frame UBO
  UBOSetPerFrame pfData;
  pfData.viewMatrix = mCurrentFrameData.viewMatrix;
  pfData.projectionMatrix = mCurrentFrameData.projectionMatrix;
  pfData.eyePos = glm::vec4(mCurrentFrameData.eyePos, 1.0);
  for( auto lI = 0u; lI < mCurrentFrameData.lightsToRender.size(); ++lI ) {
      pfData.lights[lI] = mCurrentFrameData.lightsToRender[lI];
    }
  // std::memcpy(pfData.lights, mLightsToRender.data(), mLightsToRender.size() * sizeof(ShaderLightData));
  pfData.numLights = mCurrentFrameData.lightsToRender.size();

  auto& pfUBO = imageData.ubo;
  std::memcpy(pfUBO->map(), &pfData, sizeof(UBOSetPerFrame));
  pfUBO->flush();
  pfUBO->unmap(); // TODO: Shouldn't actually being unmapping here, the buffer will stick around so this is unecesarry

  // And bind it to the pipeline
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
    mGraphicsPipeline->pipelineLayout(),
    0, 1,
    &imageData.uboDescriptor,
    0, nullptr);

  // Iterate over the meshes we need to render
  // TODO: This is a tad messy here - Should certainly
  // collate the materials into one big UBO, allow
  // meshes to dynamically add/remove from the scene etc.
  for (auto& mesh : mCurrentFrameData.meshesToRender) {
    // Bind the descriptor set for the mesh
    // Static mesh data such as Material, textures, etc
    auto meshData = mMeshRenderData.find(mesh.mesh);
    if (meshData != mMeshRenderData.end()) {
      commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        mGraphicsPipeline->pipelineLayout(),
        1, 1,
        &meshData->second.descriptorSet,
        0, nullptr);
    }
    else
    {
      // Bind the default material
      commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
        mGraphicsPipeline->pipelineLayout(),
        1, 1,
        &mDescriptorSetMeshDataDefault,
        0, nullptr);
    }

    // Set push constants - Data that changes often
    // or over time such as the model matrix
    PushConstantSet puush;
    puush.modelMatrix = mesh.modelMatrix;
    commandBuffer.pushConstants(
      mGraphicsPipeline->pipelineLayout(),
      mPushConstantSetRange.stageFlags,
      mPushConstantSetRange.offset,
      sizeof(PushConstantSet),
      &puush);

    // Set vertex buffer
    auto& vertBuf = mesh.mesh->mVertexBuffer;
    // TODO: Probably not efficient - Should batch multiple buffers/offsets here, probably based on shared materials
    vk::Buffer buffers[] = { vertBuf->buffer() };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, 1, buffers, offsets);

    auto& idxBuf = mesh.mesh->mIndexBuffer;
    // If there's no index buffer just draw all the vertices
    if (!idxBuf) {
      commandBuffer.draw(static_cast<uint32_t>(vertBuf->size() / sizeof(Vertex)), // Draw n vertices
        1, // Used for instanced rendering, 1 otherwise
        0, // First vertex
        0  // First instance
      );
    }
    else {
      // Set index buffer
      commandBuffer.bindIndexBuffer(idxBuf->buffer(), 0, vk::IndexType::eUint32);

      // Draw
      commandBuffer.drawIndexed(static_cast<uint32_t>(idxBuf->size() / sizeof(uint32_t)), 1, 0, 0, 0);
    }
  }

  // End the render pass
  commandBuffer.endRenderPass();

  // End the command buffer
  commandBuffer.end();
}

void Renderer::initDescriptorSetsForRenderer() {

  // Create a descriptor pool, to allocate descriptor sets for per-frame data
  // Just need to be able to pass one UBO for now, but as we'll have multiple
  // frames in flight we'll have several copies of the buffer
  auto poolSize = vk::DescriptorPoolSize()
    .setType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(mPerImageData.size());

  auto poolInfo = vk::DescriptorPoolCreateInfo()
    .setFlags({})
    .setMaxSets(mPerImageData.size())
    .setPoolSizeCount(1)
    .setPPoolSizes(&poolSize);
  mDescriptorPoolRenderer = mDeviceInstance->device().createDescriptorPoolUnique(poolInfo);
}

void Renderer::createDescriptorSetsForRenderer() {
  // Create the descriptor sets
  // These ones are created once and remain for the renderer's lifetime
  // Data in the UBOs will change, after synchronising with the pipeline

  std::vector<vk::DescriptorSetLayout> dsLayouts;
  for( auto i = 0u; i < mPerImageData.size(); ++i ) dsLayouts.emplace_back(mGraphicsPipeline->descriptorSetLayouts()[0].get());
  auto dsInfo = vk::DescriptorSetAllocateInfo()
    .setDescriptorPool(mDescriptorPoolRenderer.get())
    .setDescriptorSetCount(dsLayouts.size())
    .setPSetLayouts(dsLayouts.data());

  auto sets = mDeviceInstance->device().allocateDescriptorSets(dsInfo);
  for (auto i = 0u; i < mPerImageData.size(); ++i) {
    mPerImageData[i].uboDescriptor = std::move(sets[i]);
  }

  // Create UBOs
  for (auto i = 0u; i < mPerImageData.size(); ++i) {
    std::unique_ptr<SimpleBuffer> ubo(new SimpleBuffer(
      *mDeviceInstance.get(),
      sizeof(UBOSetPerFrame),
      vk::BufferUsageFlagBits::eUniformBuffer));
    ubo->name() = "Global Frame UBO " + std::to_string(i);
    mPerImageData[i].ubo = std::move(ubo);
  }

  // Write descriptors to the sets, to bind them to the UBOs
  for (auto i = 0u; i < mPerImageData.size(); ++i) {
    auto& ubo = mPerImageData[i].ubo;
    auto& ds  = mPerImageData[i].uboDescriptor;

    std::vector<vk::DescriptorBufferInfo> uInfos;
    auto uInfo = vk::DescriptorBufferInfo()
      .setBuffer(ubo->buffer())
      .setOffset(0)
      .setRange(VK_WHOLE_SIZE);
    uInfos.emplace_back(uInfo);

    auto wInfo = vk::WriteDescriptorSet()
      .setDstSet(ds)
      .setDstBinding(0)
      .setDstArrayElement(0)
      .setDescriptorCount(static_cast<uint32_t>(uInfos.size()))
      .setDescriptorType(vk::DescriptorType::eUniformBuffer)
      .setPImageInfo(nullptr)
      .setPBufferInfo(uInfos.data())
      .setPTexelBufferView(nullptr);

    mDeviceInstance->device().updateDescriptorSets(1, &wInfo, 0, nullptr);
  }
}

void Renderer::initDescriptorSetsForMeshes() {

  // Create a descriptor pool, to allocate descriptor sets for per-mesh data
  // Here we've set a resonable limit of 1000, meaning 1000 different material
  // and texture combinations within a single frame
  // TODO: At the moment we don't clear down old data, so if the scene changes
  // over time we'll run out.
  auto descriptorsPerSet = 1u;
  auto maxDescriptorSets = 1000u;
  auto poolSize = vk::DescriptorPoolSize()
    .setType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(maxDescriptorSets * descriptorsPerSet);

  auto poolInfo = vk::DescriptorPoolCreateInfo()
    .setFlags({})
    .setMaxSets(maxDescriptorSets)
    .setPoolSizeCount(1)
    .setPPoolSizes(&poolSize);
  mDescriptorPoolMeshes = mDeviceInstance->device().createDescriptorPoolUnique(poolInfo);
}

void Renderer::createDefaultDescriptorSetForMesh() {
  // Create a default descriptor set/UBO for mesh data
  // This will serve as the default material is none is available
  // on the mesh
  // TODO: Set this up with a nice 'missing texture' texture
  const vk::DescriptorSetLayout dsLayouts[] = { mGraphicsPipeline->descriptorSetLayouts()[1].get() };
  auto dsInfo = vk::DescriptorSetAllocateInfo()
    .setDescriptorPool(mDescriptorPoolMeshes.get())
    .setDescriptorSetCount(1)
    .setPSetLayouts(dsLayouts);

  auto sets = mDeviceInstance->device().allocateDescriptorSets(dsInfo);
  mDescriptorSetMeshDataDefault = std::move(sets.front());

  // Create the default material UBO
  mUBOMeshDataDefault.reset(new SimpleBuffer(
    *mDeviceInstance.get(),
    sizeof(UBOSetPerMaterial),
    vk::BufferUsageFlagBits::eUniformBuffer));
  mUBOMeshDataDefault->name() = "Material UBO (Default)";

  UBOSetPerMaterial defaultMat;
  defaultMat.baseColourFactor = glm::vec4(1.f, 0.f, 1.f, 1.f);
  std::memcpy(mUBOMeshDataDefault->map(), &defaultMat, sizeof(UBOSetPerMaterial));
  mUBOMeshDataDefault->flush();
  mUBOMeshDataDefault->unmap();

  // Write descriptors to the sets, to bind them to the UBOs
  auto& ubo = mUBOMeshDataDefault;

  std::vector<vk::DescriptorBufferInfo> uInfos;
  auto uInfo = vk::DescriptorBufferInfo()
    .setBuffer(ubo->buffer())
    .setOffset(0)
    .setRange(VK_WHOLE_SIZE);
  uInfos.emplace_back(uInfo);

  auto wInfo = vk::WriteDescriptorSet()
    .setDstSet(mDescriptorSetMeshDataDefault)
    .setDstBinding(0)
    .setDstArrayElement(0)
    .setDescriptorCount(static_cast<uint32_t>(uInfos.size()))
    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
    .setPImageInfo(nullptr)
    .setPBufferInfo(uInfos.data())
    .setPTexelBufferView(nullptr);

  mDeviceInstance->device().updateDescriptorSets(1, &wInfo, 0, nullptr);
}

void Renderer::createDescriptorSetForMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material) {
  // Ensure a descriptor set and UBO are setup in order to render the mesh/material
  auto meshData = mMeshRenderData.find(mesh);
  if (meshData != mMeshRenderData.end()) return;

  // The mesh hasn't been seen before
  // Create the necesarry descriptor set
  MeshRenderData d;

  // Setup the UBO, to contain material info
  UBOSetPerMaterial mat;
  mat.alphaCutOff = material->alphaCutOff;
  mat.baseColourFactor = material->baseColourFactor;
  mat.diffuseFactor = material->diffuseFactor;
  mat.emissiveFactor = material->emissiveFactor;
  mat.specularFactor = material->specularFactor;
  
  d.uboMaterial.reset(new SimpleBuffer(
    *mDeviceInstance.get(),
    sizeof(UBOSetPerMaterial),
    vk::BufferUsageFlagBits::eUniformBuffer)
  );
  d.uboMaterial->name() = "Mesh Material UBO";

  std::memcpy(d.uboMaterial->map(), &mat, sizeof(UBOSetPerMaterial));
  d.uboMaterial->flush();
  d.uboMaterial->unmap();

  // TODO: Magic number - This should be encapsulated somehow, or just have a value in case we
  // change the pipeline layout later (likely)
  const vk::DescriptorSetLayout dsLayouts[] = { mGraphicsPipeline->descriptorSetLayouts()[1].get() };
  auto dsInfo = vk::DescriptorSetAllocateInfo()
    .setDescriptorPool(mDescriptorPoolMeshes.get())
    .setDescriptorSetCount(1)
    .setPSetLayouts(dsLayouts);

  try {
    auto sets = mDeviceInstance->device().allocateDescriptorSets(dsInfo);
    d.descriptorSet = std::move(sets.front());
  }
  catch (vk::OutOfPoolMemoryError& e) {
    // Explicit check as the pool has a limit
    // TODO: Could auto-scale and make more pools, or free up
    // resources and try again?
    // For now just report the issue and we'll have missing textures
    // or something (TODO: Like all good engines we should have a default material for missing textures)
    // TODO TODO: We'll run out really quickly unless the materials are merged - Initially it's
    // one descriptor per mesh, and a scene could have a lot in it!
    std::cerr << "Renderer::createDescriptorSet: Ran out of space in descriptor pool" << std::endl;
    return;
  }
  
  // Update the descriptor set to map to the buffers
  std::vector<vk::DescriptorBufferInfo> uInfos;
  auto uInfo = vk::DescriptorBufferInfo()
    .setBuffer(d.uboMaterial->buffer())
    .setOffset(0)
    .setRange(VK_WHOLE_SIZE);
  uInfos.emplace_back(uInfo);

  auto wInfo = vk::WriteDescriptorSet()
    .setDstSet(d.descriptorSet)
    .setDstBinding(0)
    .setDstArrayElement(0)
    .setDescriptorCount(static_cast<uint32_t>(uInfos.size()))
    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
    .setPImageInfo(nullptr)
    .setPBufferInfo(uInfos.data())
    .setPTexelBufferView(nullptr);

  mDeviceInstance->device().updateDescriptorSets(1, &wInfo, 0, nullptr);

  mMeshRenderData[mesh] = std::move(d);
}

void Renderer::renderMesh(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material, glm::mat4x4 modelMat) {
  if( !mesh ) return;
  if( !mesh->validForRender() ) return;
  if( !material ) material.reset(new Material());

  // Note that we need to render the mesh, frameEnd will
  // submit this to the gpu as needed
  MeshRenderInstance i;
  i.mesh = mesh;
  i.modelMatrix = modelMat;

  mCurrentFrameData.meshesToRender.emplace_back(i);
  createDescriptorSetForMesh(mesh, material);
}

void Renderer::renderLight( const Light& l ) {
  if( mCurrentFrameData.lightsToRender.size() >= mGraphicsSpecConstants.maxLights ) {
    // We've hit the limit of the shaders. Probably an insane scene but handle it sensibly
    std::cerr << "WARNING: Renderer::renderLight: Reached the limit of " << mGraphicsSpecConstants.maxLights << " lights. Simplify the scene or increase Renderer::MAX_LIGHTS" << std::endl;
    return;
  }
  ShaderLightData sl;
  sl.colour = glm::vec4(l.colour, l.intensity);
  switch(l.type()) {
    case Light::Type::Directional:
      sl.posOrDir = glm::vec4(l.direction, 0.f);
      break;
    case Light::Type::Point:
      sl.posOrDir = glm::vec4(l.position, 1.f);
      break;
    case Light::Type::Spot:
      sl.posOrDir = glm::vec4(l.position, 1.f);
      break;
  }
  // x == range, y == innerConeCos, z == outerConeCos, w == Light::Type
  sl.typeAndParams = glm::vec4(
    l.range,
    std::cos(l.innerConeAngle),
    std::cos(l.outerConeAngle),
    static_cast<float>(l.type())
  );
  mCurrentFrameData.lightsToRender.emplace_back(sl);
}

bool Renderer::pollWindowEvents() {
  // Poll the events, will be handled by the GLFW callbacks
  // and passed to the mEngine's event queue
  glfwPollEvents();

  // A special case, report to the Engine that we need to close
  if (glfwWindowShouldClose(mWindow)) return false;

  return true;
}

void Renderer::frameStart() {
  // Reset any per-frame data
  mCurrentFrameData.meshesToRender.clear();
  mCurrentFrameData.lightsToRender.clear();

  // Update the per-frame uniforms
  mCurrentFrameData.viewMatrix = mEngine.camera().mViewMatrix;
  mCurrentFrameData.projectionMatrix = mEngine.camera().mProjectionMatrix;
  mCurrentFrameData.eyePos = mEngine.camera().mPosition;

  // Engine will now do its thing, we'll get calls to various
  // render methods here, then frameEnd to commit the frame

  // TODO: Placeholder for lighting stuff, will be replaced by either a default min-ambient light or entirely rely on LightNodes in the graph
  ShaderLightData l;
  l.colour = glm::vec4(1.f,1.f,1.f, 1.f);
  l.posOrDir = glm::vec4(10.f,10.f,0.f,1.f);
  l.typeAndParams.w = static_cast<float>(Light::Type::Point);
  mCurrentFrameData.lightsToRender.emplace_back(l);
}

void Renderer::frameEnd() {
  // Handle any explicit resize events from the window system
  if( mRecreateSwapChainSoon ) {
    reCreateSwapChainAndGraphicsPipeline();
    mRecreateSwapChainSoon = false;
  }

  // Important that we catch any vulkan errors here
  // Renderer assumes the default of exceptions enabled for vulkan.hpp
  // and probably won't even build with them disabled
  try
  {
    // TODO: This method is kinda complex/verbose, should simplify if possible

    // Wait until any previous runs of this frame have finished
    mDeviceInstance->device().waitForFences(1, &mPerFrameData[mCurrentFrameData.frameIndex].renderFinishedFence.get(), true, std::numeric_limits<uint64_t>::max());

    // TODO: We should perform buffer updates and such here
    // before waiting on the swapchain image's fence/performing blocking calls below

    // Acquire an image from the swap chain
    // Important: The image index will probably not match the frame index - If the gpu can empty
    // the swap chain we'll probably just get the 1st/2nd ones, with any others being rarely used.
    auto img = mDeviceInstance->device().acquireNextImageKHR(
      mWindowIntegration->swapChain(), // Get an image from this
      std::numeric_limits<uint64_t>::max(), // Don't timeout, block until an image is available (TODO: Should skip the frame and let the engine continue updating?)
      mPerFrameData[mCurrentFrameData.frameIndex].imageAvailableSem.get(), // semaphore to signal once any existing presentation tasks are done with this image, and that it's available to be presented to
      vk::Fence()); // Dummy fence, we don't care here

    if( img.result == vk::Result::eSuboptimalKHR ) {
      // This isn't an error in that we can continue rendering, but we should recreate the swapchain soon
      mRecreateSwapChainSoon = true;
    }
    mCurrentFrameData.imageIndex = img.value;

    // If we already have a fence for the image we need to wait - The last frame using this image is active
    if( mPerImageData[mCurrentFrameData.imageIndex].fence ) {
      mDeviceInstance->device().waitForFences(1, &mPerImageData[mCurrentFrameData.imageIndex].fence, true, std::numeric_limits<uint64_t>::max());
    }
    mPerImageData[mCurrentFrameData.imageIndex].fence = mPerFrameData[mCurrentFrameData.frameIndex].renderFinishedFence.get();

    // Rebuild the command buffer every frame
    // This isn't the most efficient but we're at least re-using the command buffer
    // In a most complex application we would have multiple command buffers and only rebuild
    // the section that needs changing..I think
    auto& commandBuffer = mCommandBuffers[mCurrentFrameData.imageIndex].get();
    buildCommandBuffer(
      commandBuffer,
      mFrameBuffer->frameBuffers()[mCurrentFrameData.imageIndex].get());

    // Setup synchronisation for the command buffer submission
    // - Wait until the presentation image is available before execution
    // - (dstStageMask) Perform the corresponding semaphore wait at this stage
    //   - In this case only wait once we reach colorAttachmantOutput, all other work can be done before hand
    // - Signal a semaphore when execution is done (Used later when presenting)
    vk::Semaphore waitSemaphores[]{ mPerFrameData[mCurrentFrameData.frameIndex].imageAvailableSem.get() };
    vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
    auto& renderFinishedSemaphore = mPerFrameData[mCurrentFrameData.frameIndex].renderFinishedSem.get();
    auto submitInfo = vk::SubmitInfo()
      .setWaitSemaphoreCount(1)
      .setPWaitSemaphores(waitSemaphores)
      .setPWaitDstStageMask(waitStages)
      .setCommandBufferCount(1)
      .setPCommandBuffers(&commandBuffer)
      .setSignalSemaphoreCount(1)
      .setPSignalSemaphores(&renderFinishedSemaphore)
      ;

    // Present the results of a frame to the swap chain
    // - Presentation will wait until renderFinishedSemaphores have been signalled
    vk::SwapchainKHR swapChains[] = { mWindowIntegration->swapChain() };
    auto presentInfo = vk::PresentInfoKHR()
      .setWaitSemaphoreCount(1)
      .setPWaitSemaphores(&renderFinishedSemaphore)
      .setSwapchainCount(1)
      .setPSwapchains(swapChains)
      .setPImageIndices(&mCurrentFrameData.imageIndex)
      .setPResults(nullptr)
      ;

    // Reset the frame fence
    // This fence will be signalled when the queue submissions are finished
    if( mDeviceInstance->device().resetFences(1, &mPerFrameData[mCurrentFrameData.frameIndex].renderFinishedFence.get()) != vk::Result::eSuccess ) {
            throw std::runtime_error("Renderer: Failed to reset frame fence");
    }

    // submit, signal the frame fence at the end
    auto submitResult = mQueue->queue.submit(1, &submitInfo, mPerFrameData[mCurrentFrameData.frameIndex].renderFinishedFence.get());
    if( submitResult != vk::Result::eSuccess ) {
        throw std::runtime_error("Renderer: Queue submission failed");
      }

    // TODO: Currently using a single queue for both graphics and present
    // Some systems may not be able to support this
    mQueue->queue.presentKHR(presentInfo);

    // Advance to next frame index, loop at max
    mCurrentFrameData.frameIndex++;
    if (mCurrentFrameData.frameIndex == mMaxFramesInFlight) mCurrentFrameData.frameIndex = 0;
  }
  catch ( vk::OutOfDateKHRError& e ) {
    // Either acquireNextImageKHR or presentKHR returned OutOfDate, meaning a window resize/similar
    reCreateSwapChainAndGraphicsPipeline();
  }
  catch (vk::Error& e) {
    std::cerr << "Renderer::frameEnd: vk::Error: " << e.what() << std::endl;
    throw;
  }
  catch (...) {
    std::cerr << "Renderer::frameEnd: Unknown Exception:" << std::endl;
    throw;
  }
}

void Renderer::waitIdle() {
  mDeviceInstance->waitAllDevicesIdle();
}

void Renderer::cleanup() {
  // Explicitly cleanup the vulkan objects here
  // Ensure they are shut down before terminating glfw
  // TODO: Could wrap the glfw stuff in a smart pointer and
  // remove the need for this method

  if (!mDeviceInstance) {
    // Already cleaned up, or otherwise invalid
    return;
  }
  mDeviceInstance->waitAllDevicesIdle();

  mUBOMeshDataDefault.reset();
  mMeshRenderData.clear();
  mPerImageData.clear();
  mPerFrameData.clear();

  mCommandBuffers.clear();
  mCommandPool.reset();

  mDescriptorPoolMeshes.reset();
  mDescriptorPoolRenderer.reset();

  mGraphicsPipeline.reset();
  mFrameBuffer.reset();
  mWindowIntegration.reset();
  mDeviceInstance.reset();

  glfwDestroyWindow(mWindow);
  glfwTerminate();
}


std::unique_ptr<SimpleBuffer> Renderer::createSimpleVertexBuffer(std::vector<Vertex> verts) {
  std::unique_ptr<SimpleBuffer> result(new SimpleBuffer(
    *mDeviceInstance.get(),
    verts.size() * sizeof(decltype(verts)::value_type),
    vk::BufferUsageFlagBits::eVertexBuffer));
  result->name() = "Renderer::createSimpleVertexBuffer";
  return result;
}

std::unique_ptr<SimpleBuffer> Renderer::createSimpleIndexBuffer(std::vector<uint32_t> indices) {
  std::unique_ptr<SimpleBuffer> result(new SimpleBuffer(
    *mDeviceInstance.get(),
    indices.size() * sizeof(decltype(indices)::value_type),
    vk::BufferUsageFlagBits::eIndexBuffer));
  result->name() = "Renderer::createSimpleIndexBuffer";
  return result;
}
