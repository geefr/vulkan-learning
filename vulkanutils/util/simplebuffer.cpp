#include "simplebuffer.h"

#include "deviceinstance.h"

SimpleBuffer::SimpleBuffer(
    DeviceInstance& deviceInstance,
    vk::DeviceSize size,
    vk::BufferUsageFlags usageFlags,
    vk::MemoryPropertyFlags memFlags)
  : mDeviceInstance(deviceInstance)
  , mSize(size)
  , mBufferUsageFlags(usageFlags)
  , mMemoryPropertyFlags(memFlags)
{
  mBuffer =  mDeviceInstance.createBuffer(mSize, mBufferUsageFlags);
  mDeviceMemory =  mDeviceInstance.allocateDeviceMemoryForBuffer(mBuffer.get(), mMemoryPropertyFlags);
  mDeviceInstance.bindMemoryToBuffer(mBuffer.get(), mDeviceMemory.get(), 0);
}

SimpleBuffer::~SimpleBuffer() {
  if( mMapped) {
    flush();
    unmap();
  }
}

void* SimpleBuffer::map() {
  if( mMapped ) return nullptr;
  return  mDeviceInstance.mapMemory(mDeviceMemory.get(), 0, VK_WHOLE_SIZE);
}

void SimpleBuffer::unmap() {
  if( !mMapped ) return;
   mDeviceInstance.unmapMemory(mDeviceMemory.get());
}

void SimpleBuffer::flush() {
   mDeviceInstance.flushMemoryRanges({vk::MappedMemoryRange(mDeviceMemory.get(), 0, VK_WHOLE_SIZE)});
}
