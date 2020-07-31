/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#include "simpleimage.h"

#include "deviceinstance.h"

SimpleImage::SimpleImage(DeviceInstance& deviceInstance,
    vk::ImageType imageType,
    vk::ImageViewType imageViewType,
    vk::Format imageFormat,
    vk::Extent3D extent,
    uint32_t mipLevels,
    uint32_t arrayLayers,
    vk::SampleCountFlagBits samples,
    vk::ImageUsageFlags usageFlags,
    vk::MemoryPropertyFlags memFlags,
    vk::ImageAspectFlags aspectFlags)
  : mDeviceInstance(deviceInstance)
{
  if( mipLevels == 0 ) throw std::runtime_error("SimpleImage: mipLevels must be >= 1");
  if( arrayLayers == 0 ) throw std::runtime_error("SimpleImage: arrayLayers must be >= 1");
  // Create the image
  mImage = mDeviceInstance.createImage(
        imageType,
        imageFormat,
        extent,
        mipLevels,
        arrayLayers,
        samples,
        vk::ImageTiling::eOptimal,
        usageFlags,
        vk::SharingMode::eExclusive,
        {},
        vk::ImageLayout::eUndefined
  );

  // Setup the image's memory
  mDeviceMemory = mDeviceInstance.allocateDeviceMemoryForImage(mImage, memFlags);
  if( !mDeviceMemory ) throw std::runtime_error("SimpleImage: Failed to allocate memory");
  mDeviceInstance.bindMemoryToImage(mImage, mDeviceMemory, 0);

  // TODO: For now just making one view, same format as the image itself
  mImageView = mDeviceInstance.createImageView(
        mImage.get(),
        imageViewType,
        imageFormat,
        aspectFlags
        );
  if( !mImageView ) throw std::runtime_error("SimpleImage: Failed to create image view");
  // TODO: Image layout/transitions needed for texture upload
}

SimpleImage::~SimpleImage() {
  if( mMapped) {
    flush();
    unmap();
  }
}

void* SimpleImage::map() {
  if( mMapped ) return nullptr;
  auto res = mDeviceInstance.mapMemory(mDeviceMemory.get(), 0, VK_WHOLE_SIZE);
  mMapped = true;
  return res;
}

void SimpleImage::unmap() {
  if( !mMapped ) return;
   mDeviceInstance.unmapMemory(mDeviceMemory.get());
   mMapped = false;
}

void SimpleImage::flush() {
   mDeviceInstance.flushMemoryRanges({vk::MappedMemoryRange(mDeviceMemory.get(), 0, VK_WHOLE_SIZE)});
}

std::string& SimpleImage::name() { return mName; }
