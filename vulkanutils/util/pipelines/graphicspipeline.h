#ifndef GRAPHICSPIPELINE_H
#define GRAPHICSPIPELINE_H

#include <vulkan/vulkan.hpp>

#include "pipeline.h"

class DeviceInstance;
class WindowIntegration;

/**
 * Class to manage stuff around the vulkan graphics pipeline
 * TODO: This isn't flexible, at first holding the pipeline and render passes together, which might not be that sensible?
 */
class GraphicsPipeline : public Pipeline
{
public:
  struct Shaders
  {
    vk::ShaderModule mVertexShader;
    vk::ShaderModule mFragmentShader;
  };

  GraphicsPipeline(WindowIntegration& windowIntegration, DeviceInstance& deviceInstance);
  virtual ~GraphicsPipeline() final override {}

  /// Shaders to use. Handles may be destroyed after calling build()
  /// Caller should call shaders() = {} to reset, but it's not required
  GraphicsPipeline::Shaders& shaders() { return mShaders; }

  vk::RenderPass& renderPass() { return mRenderPass.get(); }

  /// Vertex input bindings
  std::vector<vk::VertexInputBindingDescription>& vertexInputBindings() { return mVertexInputBindings; }
  /// Vertex input attribute descriptions
  std::vector<vk::VertexInputAttributeDescription>& vertexInputAttributes() { return mVertexInputAttributes; }


  void inputAssembly_primitiveTopology(vk::PrimitiveTopology top) { mInputAssemblyPrimitiveTopology = top; }

private:
  void createRenderPass();
  void createPipeline() final override;
  std::vector<vk::PipelineShaderStageCreateInfo> createShaderStageInfo() final override;

  WindowIntegration& mWindowIntegration;

  GraphicsPipeline::Shaders mShaders;
  std::vector<vk::VertexInputBindingDescription> mVertexInputBindings;
  std::vector<vk::VertexInputAttributeDescription> mVertexInputAttributes;

  vk::UniqueRenderPass mRenderPass;

  // Input assembly settings
  vk::PrimitiveTopology mInputAssemblyPrimitiveTopology = vk::PrimitiveTopology::eTriangleList;
};

#endif // GRAPHICSPIPELINE_H
