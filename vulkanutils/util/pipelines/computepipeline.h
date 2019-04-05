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

private:
  void createPipeline() final override;
  void createDescriptorSetLayouts();
};

#endif
