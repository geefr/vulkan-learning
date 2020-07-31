/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

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
class GraphicsPipeline final : public Pipeline
{
public:
  GraphicsPipeline(WindowIntegration& windowIntegration, DeviceInstance& deviceInstance, bool invertY = false);
  virtual ~GraphicsPipeline() final override {}

  vk::RenderPass& renderPass() { return mRenderPass.get(); }

  /// Vertex input bindings
  std::vector<vk::VertexInputBindingDescription>& vertexInputBindings() { return mVertexInputBindings; }
  /// Vertex input attribute descriptions
  std::vector<vk::VertexInputAttributeDescription>& vertexInputAttributes() { return mVertexInputAttributes; }

  void inputAssembly_primitiveTopology(vk::PrimitiveTopology top) { mInputAssemblyPrimitiveTopology = top; }

private:
  void createRenderPass();
  void createPipeline() final override;

  WindowIntegration& mWindowIntegration;

  std::vector<vk::VertexInputBindingDescription> mVertexInputBindings;
  std::vector<vk::VertexInputAttributeDescription> mVertexInputAttributes;

  vk::UniqueRenderPass mRenderPass;

  // Input assembly settings
  vk::PrimitiveTopology mInputAssemblyPrimitiveTopology = vk::PrimitiveTopology::eTriangleList;

  // Whether to flip y axis (follow opengl conventions) or not (follow vulkan conventions)
  bool mInvertY = false;

  // TODO: To support multisampling we'll need to render into a multisampled buffer, then render that to the single-sampled framebuffer
  // See https://vulkan-tutorial.com/Multisampling
  vk::SampleCountFlagBits mSampleCount = vk::SampleCountFlagBits::e1;
};

#endif // GRAPHICSPIPELINE_H
