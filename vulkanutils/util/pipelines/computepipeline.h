#ifndef COMPUTEPIPELINE_H
#define COMPUTEPIPELINE_H

#include <vulkan/vulkan.hpp>

#include "pipeline.h"

#include <vector>

class DeviceInstance;

/**
 * Class to setup/manage a compute pipeline
 */
class ComputePipeline : public Pipeline
{
public:
  ComputePipeline(DeviceInstance& deviceInstance);
  virtual ~ComputePipeline() final override {}

  /// Shader to build into the pipeline
  vk::ShaderModule& shader() { return mShader; }

  vk::Pipeline& pipeline() { return mPipeline.get(); }
  vk::PipelineLayout& pipelineLayout() { return mPipelineLayout.get(); }

private:
  void createPipeline() final override;
  void createDescriptorSetLayouts();
  std::vector<vk::PipelineShaderStageCreateInfo> createShaderStageInfo() final override;

  vk::ShaderModule mShader;
};

#endif
