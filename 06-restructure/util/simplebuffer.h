#ifndef SIMPLEBUFFER_H
#define SIMPLEBUFFER_H

#include <vulkan/vulkan.hpp>

/**
 * A very simple class for managing buffers
 * - One buffer for each piece of data
 * - Absolutely nothing else
 *
 * This really won't scale as we'll hit the allocation limit
 * at some point
 * But for now let's just roll with it as a starting point
 */
class SimpleBuffer
{
public:
  /**
   * Allocate a buffer
   * Memory will be immediately allocated and bound to the buffer
   * Each buffer will have a separate memory allocation
   */
  SimpleBuffer(
      vk::DeviceSize size,
      vk::BufferUsageFlags usageFlags,
      vk::MemoryPropertyFlags memFlags = {vk::MemoryPropertyFlagBits::eHostVisible});
  ~SimpleBuffer();

  void* map();
  void unmap();
  void flush();

  vk::Buffer& buffer();

private:
  SimpleBuffer() = delete;
  vk::UniqueBuffer mBuffer;
  vk::UniqueDeviceMemory mDeviceMemory;
  vk::DeviceSize mSize;
  vk::BufferUsageFlags mBufferUsageFlags;
  vk::MemoryPropertyFlags mMemoryPropertyFlags;

  bool mMapped = false;
};

inline vk::Buffer& SimpleBuffer::buffer() { return mBuffer.get(); }

#endif
