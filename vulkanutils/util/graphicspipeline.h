#ifndef GRAPHICSPIPELINE_H
#define GRAPHICSPIPELINE_H

#include <vulkan/vulkan.hpp>

class DeviceInstance;
class WindowIntegration;

/**
 * Class to manage stuff around the vulkan graphics pipeline
 * TODO: This isn't flexible, at first holding the pipeline and render passes together, which might not be that sensible?
 */
class GraphicsPipeline
{
public:
  struct Shaders
  {
    vk::ShaderModule mVertexShader;
    vk::ShaderModule mFragmentShader;
  };

  GraphicsPipeline(WindowIntegration& windowIntegration, DeviceInstance& deviceInstance);

  /**
   * Initialisation
   *
   * Pipeline will be built using the current state of the class paramters, ensure
   * all paremeters are correct before calling this method
   *
   * TODO: At firs this method can only be called once
   */
  vk::Pipeline& build();

  /**
   * Load a shader from file and build a shader module
   *
   * The caller is responsible for management of the returned module
   * It must not be deleted until all pipelines which reference the shader
   * have been initialised.
   */
  vk::UniqueShaderModule createShaderModule(const std::string& fileName);

  /// Shaders to use. Handles may be destroyed after calling build()
  /// Caller should call shaders() = {} to reset, but it's not required
  GraphicsPipeline::Shaders& shaders() { return mShaders; }

  vk::RenderPass& renderPass() { return mRenderPass.get(); }
  vk::Pipeline& pipeline() { return mPipeline.get(); }
  vk::PipelineLayout& pipelineLayout() { return mPipelineLayout.get(); }

  /// Vertex input bindings
  std::vector<vk::VertexInputBindingDescription>& vertexInputBindings() { return mVertexInputBindings; }
  /// Vertex input attribute descriptions
  std::vector<vk::VertexInputAttributeDescription>& vertexInputAttributes() { return mVertexInputAttributes; }
  /// Push Constants
  std::vector<vk::PushConstantRange>& pushConstants() { return mPushConstants; }

  void inputAssembly_primitiveTopology(vk::PrimitiveTopology top) { mInputAssemblyPrimitiveTopology = top; }

private:
  void createRenderPass();
  void createGraphicsPipeline();
  std::vector<vk::PipelineShaderStageCreateInfo> createShaderStageInfo();

  WindowIntegration& mWindowIntegration;
  DeviceInstance& mDeviceInstance;

  GraphicsPipeline::Shaders mShaders;
  std::vector<vk::VertexInputBindingDescription> mVertexInputBindings;
  std::vector<vk::VertexInputAttributeDescription> mVertexInputAttributes;
  std::vector<vk::PushConstantRange> mPushConstants;

  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniqueRenderPass mRenderPass;
  vk::UniquePipeline mPipeline;

  // Input assembly settings
  vk::PrimitiveTopology mInputAssemblyPrimitiveTopology = vk::PrimitiveTopology::eTriangleList;
};

#endif // GRAPHICSPIPELINE_H
