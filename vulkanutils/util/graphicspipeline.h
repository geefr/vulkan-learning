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
  GraphicsPipeline(WindowIntegration& windowIntegration, DeviceInstance& deviceInstance);

  vk::RenderPass& renderPass() { return mRenderPass.get(); };
  vk::UniquePipeline& graphicsPipeline() { return mGraphicsPipeline; }
  vk::UniqueShaderModule createShaderModule(const std::string& fileName);

private:
  void createRenderPass();
  void createGraphicsPipeline();

  WindowIntegration& mWindowIntegration;
  DeviceInstance& mDeviceInstance;

  vk::UniquePipelineLayout mPipelineLayout;
  vk::UniqueRenderPass mRenderPass;
  vk::UniquePipeline mGraphicsPipeline;
};

#endif // GRAPHICSPIPELINE_H
