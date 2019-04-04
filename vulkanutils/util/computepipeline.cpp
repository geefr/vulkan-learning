#include "computepipeline.h"
#include "deviceinstance.h"
#include "util.h"

#include <vulkan/vulkan.hpp>

#include <map>

ComputePipeline::ComputePipeline(DeviceInstance& deviceInstance)
  : mDeviceInstance(deviceInstance) {

}

vk::Pipeline& ComputePipeline::build() {
  createPipeline();
  return mPipeline.get();
}

void ComputePipeline::createPipeline() {
  // Later we just need to hand a pile of shaders to the pipeline
  auto shaderStage = createShaderStageInfo();
  createDescriptorSetLayouts();

  // Pipeline layout
  // Pipeline layout is where uniforms and such go
  // and they have to be known when the pipeline is built
  // So no randomly chucking uniforms around like we do in gl right?
  auto numPushConstantRanges = static_cast<uint32_t>(mPushConstants.size());
  auto numDSLayouts = static_cast<uint32_t>(mDescriptorSetLayouts.size());
  // :/
  std::vector<vk::DescriptorSetLayout> tmpLayouts;
  for( auto& p : mDescriptorSetLayouts ) tmpLayouts.emplace_back(p.get());

  auto layoutInfo = vk::PipelineLayoutCreateInfo()
      .setFlags({})
      .setSetLayoutCount(numDSLayouts)
      .setPSetLayouts(numDSLayouts ? tmpLayouts.data() : nullptr)
      .setPushConstantRangeCount(numPushConstantRanges)
      .setPPushConstantRanges(numPushConstantRanges ? mPushConstants.data() : nullptr)
      ;
  mPipelineLayout = mDeviceInstance.device().createPipelineLayoutUnique(layoutInfo);


  // Finally let's make the pipeline itself
  auto pipelineInfo = vk::ComputePipelineCreateInfo()
      .setFlags({})
      .setStage(shaderStage)
      .setLayout(mPipelineLayout.get())
      .setBasePipelineHandle({})
      .setBasePipelineIndex(-1)
      ;

  mPipeline = mDeviceInstance.device().createComputePipelineUnique({}, pipelineInfo);
}

vk::UniqueShaderModule ComputePipeline::createShaderModule(const std::string& fileName) {
  auto shaderCode = Util::readFile(fileName);
  // Note that data passed to info is as uint32_t*, so must be 4-byte aligned
  // According to tutorial std::vector already satisfies worst case alignment needs
  // TODO: But we should probably double check the alignment here just incase
  auto info = vk::ShaderModuleCreateInfo()
      .setFlags({})
      .setCodeSize(shaderCode.size())
      .setPCode(reinterpret_cast<const uint32_t*>(shaderCode.data()))
      ;
  return mDeviceInstance.device().createShaderModuleUnique(info);
}

vk::PipelineShaderStageCreateInfo ComputePipeline::createShaderStageInfo() {
  std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
  if( mShader ) {
    auto vertInfo = vk::PipelineShaderStageCreateInfo()
        .setFlags({})
        .setStage(vk::ShaderStageFlagBits::eCompute)
        .setModule(mShader)
        .setPName("main")
        .setPSpecializationInfo(nullptr);
    return vertInfo;
  }
  return {};
}

void ComputePipeline::addDescriptorSetLayoutBinding( uint32_t layoutIndex, uint32_t binding, vk::DescriptorType type, uint32_t count, vk::ShaderStageFlags stageFlags) {
  auto dslBinding = vk::DescriptorSetLayoutBinding()
      .setBinding(binding)
      .setDescriptorType(type)
      .setDescriptorCount(count)
      .setStageFlags(stageFlags);
  if( mDescriptorSetLayoutBindings.size() <= layoutIndex ) mDescriptorSetLayoutBindings.resize(layoutIndex + 1);
  mDescriptorSetLayoutBindings[layoutIndex].emplace_back(dslBinding);
}

void ComputePipeline::createDescriptorSetLayouts() {
  for( auto& dslB : mDescriptorSetLayoutBindings ) {
    auto createInfo = vk::DescriptorSetLayoutCreateInfo()
        .setBindingCount(dslB.size())
        .setPBindings(dslB.data());
    mDescriptorSetLayouts.emplace_back(mDeviceInstance.device().createDescriptorSetLayoutUnique(createInfo));
  }
}



