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
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan Experiment", nullptr, nullptr);

  // Initialise event handlers
  glfwSetWindowUserPointer(mWindow, this);
  glfwSetKeyCallback(mWindow, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
    static_cast<Renderer*>(glfwGetWindowUserPointer(win))->onGLFWKeyEvent(key, scancode, action, mods);
    });

}

void Renderer::onGLFWKeyEvent(int key, int scancode, int action, int mods) {
  if (action == GLFW_PRESS) {
    mEngine.addEvent(new KeyPressEvent(key));
  }
  else if (action == GLFW_RELEASE) {
    mEngine.addEvent(new KeyReleaseEvent(key));
  }
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

  // Create a logical device to interact with
  // To do this we also need to specify how many queues from which families we want to create
  // In this case just 1 queue from the first family which supports graphics
  mWindowIntegration.reset(new WindowIntegration(mWindow, *mDeviceInstance.get(), *mQueue));

  // Create the pipeline, with a flag to invert the viewport height (Switch to left handed coordinate system)
  // If changing this check the compile flags for GLM_FORCE_LEFT_HANDED - The rest of the engine uses one cs
  // and the renderer should handle it
  mGraphicsPipeline.reset(new GraphicsPipeline(*mWindowIntegration.get(), *mDeviceInstance.get(), false));

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

    // Setup descriptor layouts on pipeline
    // Create descriptor pools
    initDescriptorSetsForRenderer();
    initDescriptorSetsForMeshes();
    
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

  // Setup our sync primitives
  // imageAvailable - gpu: Used to stall the pipeline until the presentation has finished reading from the image
  // renderFinished - gpu: Used to stall presentation until the pipeline is finished
  // frameInFlightFence - cpu: Used to ensure we don't schedule a second frame for each image until the last is complete

  // Create the semaphores we need to manage the frames in flight
  // and any other data we need to separate on a per-frame basis
  mMaxFramesInFlight = mWindowIntegration->swapChainImages().size();
  for (auto i = 0u; i < mMaxFramesInFlight; ++i) {
    mImageAvailableSemaphores.emplace_back(mDeviceInstance->device().createSemaphoreUnique({}));
    mRenderFinishedSemaphores.emplace_back(mDeviceInstance->device().createSemaphoreUnique({}));
    // Create fence in signalled state so first wait immediately returns and resets fence
    mFrameInFlightFences.emplace_back(mDeviceInstance->device().createFenceUnique({ vk::FenceCreateFlagBits::eSignaled }));
  }

  // Create UBOs for per-frame data
  // and any defaults needed for materials/mesh data
  createDescriptorSetsForRenderer();
  createDefaultDescriptorSetForMesh();

  // TODO: Misfit
  mCommandPool = mDeviceInstance->createCommandPool({ vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    /* vk::CommandPoolCreateFlagBits::eTransient | // Buffers will be short-lived and returned to pool shortly after use
       vk::CommandPoolCreateFlagBits::eResetCommandBuffer // Buffers can be reset individually, instead of needing to reset the entire pool
       */
    }, *mQueue);

  // Now make a command buffer for each framebuffer
  auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo()
    .setCommandPool(mCommandPool.get())
    .setCommandBufferCount(static_cast<uint32_t>(mWindowIntegration->swapChainImages().size()))
    .setLevel(vk::CommandBufferLevel::ePrimary)
    ;

  mCommandBuffers = mDeviceInstance->device().allocateCommandBuffersUnique(commandBufferAllocateInfo);
}

void Renderer::buildCommandBuffer(vk::CommandBuffer& commandBuffer, const vk::Framebuffer& frameBuffer, uint32_t frameIndex) {

  auto beginInfo = vk::CommandBufferBeginInfo()
    .setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse) // Buffer can be resubmitted while already pending execution
    .setPInheritanceInfo(nullptr)
    ;
  commandBuffer.begin(beginInfo);

  // Start the render pass
  // Info for attachment load op clear
  std::array<float, 4> col = { 0.f,.0f,0.f,1.f };
  vk::ClearValue clearColour(col);

  auto renderPassInfo = vk::RenderPassBeginInfo()
    .setRenderPass(mGraphicsPipeline->renderPass())
    .setFramebuffer(frameBuffer)
    .setClearValueCount(1)
    .setPClearValues(&clearColour);
  renderPassInfo.renderArea.offset = vk::Offset2D(0, 0);
  renderPassInfo.renderArea.extent = mWindowIntegration->extent();

  // render commands will be embedded in primary buffer and no secondary command buffers
  // will be executed
  commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

  // Bind the graphics pipeline
  commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mGraphicsPipeline->pipeline());

  // Update the per-frame UBO
  UBOSetPerFrame pfData;
  pfData.viewMatrix = mViewMatrix;
  pfData.projectionMatrix = mProjectionMatrix;
  pfData.eyePos = glm::vec4(mEyePos, 1.0);
  for( auto lI = 0u; lI < mLightsToRender.size(); ++lI ) {
      pfData.lights[lI] = mLightsToRender[lI];
    }
  // std::memcpy(pfData.lights, mLightsToRender.data(), mLightsToRender.size() * sizeof(ShaderLightData));
  pfData.numLights = mLightsToRender.size();

  auto& pfUBO = mUBOPerFrameInFlight[frameIndex];
  std::memcpy(pfUBO->map(), &pfData, sizeof(UBOSetPerFrame));
  pfUBO->flush();
  pfUBO->unmap(); // TODO: Shouldn't actually being unmapping here, the buffer will stick around so this is unecesarry

  // And bind it to the pipeline
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
    mGraphicsPipeline->pipelineLayout(),
    0, 1,
    &mDescriptorSetsRenderer[frameIndex],
    0, nullptr);

  // Iterate over the meshes we need to render
  // TODO: This is a tad messy here - Should certainly
  // collate the materials into one big UBO, allow
  // meshes to dynamically add/remove from the scene etc.
  for (auto& mesh : mFrameMeshesToRender) {
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
  // Register the Descriptor set layout on the pipeline
  mGraphicsPipeline->addDescriptorSetLayoutBinding(0, 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics);

  // Create a descriptor pool, to allocate descriptor sets for per-frame data
  // Just need to be able to pass one UBO for now, but as we'll have multiple
  // frames in flight we'll have several copies of the buffer
  auto poolSize = vk::DescriptorPoolSize()
    .setType(vk::DescriptorType::eUniformBuffer)
    .setDescriptorCount(mMaxFramesInFlight);

  auto poolInfo = vk::DescriptorPoolCreateInfo()
    .setFlags({})
    .setMaxSets(mMaxFramesInFlight)
    .setPoolSizeCount(1)
    .setPPoolSizes(&poolSize);
  mDescriptorPoolRenderer = mDeviceInstance->device().createDescriptorPoolUnique(poolInfo);
}

void Renderer::createDescriptorSetsForRenderer() {
  // Create the descriptor sets
  // These ones are created once and remain for the renderer's lifetime
  // Data in the UBOs will change, after synchronising with the pipeline

  std::vector<vk::DescriptorSetLayout> dsLayouts;
  for( auto i = 0u; i < mMaxFramesInFlight; ++i ) dsLayouts.emplace_back(mGraphicsPipeline->descriptorSetLayouts()[0].get());
  auto dsInfo = vk::DescriptorSetAllocateInfo()
    .setDescriptorPool(mDescriptorPoolRenderer.get())
    .setDescriptorSetCount(dsLayouts.size())
    .setPSetLayouts(dsLayouts.data());

  auto sets = mDeviceInstance->device().allocateDescriptorSets(dsInfo);
  mDescriptorSetsRenderer = std::vector<vk::DescriptorSet>(
    std::make_move_iterator(sets.begin()),
    std::make_move_iterator(sets.end())
  );

  // Create UBOs
  for (auto i = 0u; i < mMaxFramesInFlight; ++i) {
    std::unique_ptr<SimpleBuffer> pfUBO(new SimpleBuffer(
      *mDeviceInstance.get(),
      sizeof(UBOSetPerFrame),
      vk::BufferUsageFlagBits::eUniformBuffer));
    pfUBO->name() = "Per-Frame UBO " + std::to_string(i);
    mUBOPerFrameInFlight.emplace_back(std::move(pfUBO));
  }

  // Write descriptors to the sets, to bind them to the UBOs
  for (auto i = 0u; i < mMaxFramesInFlight; ++i) {
    auto& ubo = mUBOPerFrameInFlight[i];

    std::vector<vk::DescriptorBufferInfo> uInfos;
    auto uInfo = vk::DescriptorBufferInfo()
      .setBuffer(ubo->buffer())
      .setOffset(0)
      .setRange(VK_WHOLE_SIZE);
    uInfos.emplace_back(uInfo);

    auto wInfo = vk::WriteDescriptorSet()
      .setDstSet(mDescriptorSetsRenderer[i])
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
  // Register the Descriptor set layout on the pipeline
  mGraphicsPipeline->addDescriptorSetLayoutBinding(1, 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eAllGraphics);

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
  if (!mesh) return;
  if( !material ) material.reset(new Material());

  // Note that we need to render the mesh, frameEnd will
  // submit this to the gpu as needed
  MeshRenderInstance i;
  i.mesh = mesh;
  i.modelMatrix = modelMat;

  mFrameMeshesToRender.emplace_back(i);

  createDescriptorSetForMesh(mesh, material);
}

void Renderer::renderLight( const Light& l ) {
  if( mLightsToRender.size() >= mGraphicsSpecConstants.maxLights ) {
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
  mLightsToRender.emplace_back(sl);
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
  mFrameMeshesToRender.clear();
  mLightsToRender.clear();

  // Update the per-frame uniforms
  mViewMatrix = mEngine.camera().mViewMatrix;
  mProjectionMatrix = mEngine.camera().mProjectionMatrix;
  mEyePos = mEngine.camera().mPosition;

  // Engine will now do its thing, we'll get calls to various
  // render methods here, then frameEnd to commit the frame





  // TODO: Placeholder for lighting stuff
  ShaderLightData l;
  l.colour = glm::vec4(0.8f,0.5f,0.2f, 1.f);
  l.posOrDir = glm::vec4(10.f,10.f,0.f,1.f);
  l.typeAndParams.w = static_cast<float>(Light::Type::Point);
  mLightsToRender.emplace_back(l);


}

void Renderer::frameEnd() {
  // Important that we catch any vulkan errors here - it's the most likely location
  // vulkan.hpp throws exceptions by default, in release they might be disabled by CMake
  try
  {
    auto frameIndex = 0u;

    static std::once_flag windowShown;
    std::call_once(windowShown, [&win = mWindow]() {glfwShowWindow(win); });

    // Wait for the last frame to finish rendering
    mDeviceInstance->device().waitForFences(1, &mFrameInFlightFences[frameIndex].get(), true, std::numeric_limits<uint64_t>::max());

    // Advance to next frame index, loop at max
    frameIndex++;
    if (frameIndex == mMaxFramesInFlight) frameIndex = 0;

    // Reset the fence - fences must be reset before being submitted
    auto frameFence = mFrameInFlightFences[frameIndex].get();
    mDeviceInstance->device().resetFences(1, &frameFence);

    // Acquire and image from the swap chain
    auto imageIndex = mDeviceInstance->device().acquireNextImageKHR(
      mWindowIntegration->swapChain(), // Get an image from this
      std::numeric_limits<uint64_t>::max(), // Don't timeout
      mImageAvailableSemaphores[frameIndex].get(), // semaphore to signal once presentation is finished with the image
      vk::Fence()).value; // Dummy fence, we don't care here

  // Submit the command buffer
    vk::SubmitInfo submitInfo = {};
    auto commandBuffer = mCommandBuffers[imageIndex].get();
    auto frameBuffer = mFrameBuffer->frameBuffers()[imageIndex].get();

    // Rebuild the command buffer every frame
    // This isn't the most efficient but we're at least re-using the command buffer
    // In a most complex application we would have multiple command buffers and only rebuild
    // the section that needs changing..I think
    buildCommandBuffer(commandBuffer, frameBuffer, frameIndex);

    // Don't execute until this is ready
    vk::Semaphore waitSemaphores[] = { mImageAvailableSemaphores[frameIndex].get() };
    // place the wait before writing to the colour attachment

    // If we have render pass depedencies
    vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };

    // Otherwise wait until the pipeline starts to allow reading?
    //vk::PipelineStageFlags waitStages[] {vk::PipelineStageFlagBits::eTopOfPipe};


    // Signal this semaphore when rendering is done
    vk::Semaphore signalSemaphores[] = { mRenderFinishedSemaphores[frameIndex].get() };
    submitInfo.setWaitSemaphoreCount(1)
      .setPWaitSemaphores(waitSemaphores)
      .setPWaitDstStageMask(waitStages)
      .setCommandBufferCount(1)
      .setPCommandBuffers(&commandBuffer)
      .setSignalSemaphoreCount(1)
      .setPSignalSemaphores(signalSemaphores)
      ;

    vk::ArrayProxy<vk::SubmitInfo> submits(submitInfo);
    // submit, signal the frame fence at the end
    mQueue->queue.submit(submits.size(), submits.data(), frameFence);

    // Present the results of a frame to the swap chain
    vk::SwapchainKHR swapChains[] = { mWindowIntegration->swapChain() };
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.setWaitSemaphoreCount(1)
      .setPWaitSemaphores(signalSemaphores) // Wait before presentation can start
      .setSwapchainCount(1)
      .setPSwapchains(swapChains)
      .setPImageIndices(&imageIndex)
      .setPResults(nullptr)
      ;

    // TODO: Currently using a single queue for both graphics and present
    // Some systems may not be able to support this
    mQueue->queue.presentKHR(presentInfo);
  }
  catch (vk::Error& e) {
    std::cerr << "Renderer::frameEnd: vk::Error: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "Renderer::frameEnd: Unknown Exception:" << std::endl;
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
  mUBOPerFrameInFlight.clear();
  
  mFrameInFlightFences.clear();
  mRenderFinishedSemaphores.clear();
  mImageAvailableSemaphores.clear();

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
