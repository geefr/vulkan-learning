/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef SIMPLEIMAGE_H
#define SIMPLEIMAGE_H

#include <vulkan/vulkan.hpp>
#include <string>

class DeviceInstance;

/**
 * Equivalent of SimpleBuffer, but for managing images/image views
 */
class SimpleImage
{
public:
  /**
   * Allocate an image
   * Memory will be immediately allocated and bound to the image
   */
  SimpleImage(DeviceInstance& deviceInstance,
    vk::ImageType imageType,
    vk::ImageViewType imageViewType,
    vk::Format imageFormat,
    vk::Extent3D extent,
    uint32_t mipLevels,
    uint32_t arrayLayers,
    vk::SampleCountFlagBits samples,
    vk::ImageUsageFlags usageFlags,
    vk::MemoryPropertyFlags memFlags = vk::MemoryPropertyFlagBits::eDeviceLocal,
    vk::ImageAspectFlags aspectFlags = {}
  );

  ~SimpleImage();

  void* map();
  void unmap();
  void flush();

  vk::Image& image();
  vk::ImageView& view();

  vk::Format format() const;

  /// The name of the image - Handy if you're trying to work out which one you forgot to delete ;)
  std::string& name();

private:
  SimpleImage() = delete;

  DeviceInstance& mDeviceInstance;
  vk::UniqueImage mImage;
  vk::UniqueImageView mImageView;

  vk::UniqueDeviceMemory mDeviceMemory;

  vk::Format mFormat;

  bool mMapped = false;
  std::string mName;
};

inline vk::Image& SimpleImage::image() { return mImage.get(); }
inline vk::ImageView& SimpleImage::view() { return mImageView.get(); }

#endif
