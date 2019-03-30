#include "simplebuffer.h"

#include "registrar.h"

SimpleBuffer::SimpleBuffer(
    vk::DeviceSize size,
    vk::BufferUsageFlags usageFlags,
    vk::MemoryPropertyFlags memFlags)
  : mSize(size)
  , mBufferUsageFlags(usageFlags)
  , mMemoryPropertyFlags(memFlags)
{
  mBuffer = Registrar::singleton().createBuffer(mSize, mBufferUsageFlags);
  mDeviceMemory = Registrar::singleton().allocateDeviceMemoryForBuffer(mBuffer.get(), mMemoryPropertyFlags);
  Registrar::singleton().bindMemoryToBuffer(mBuffer.get(), mDeviceMemory.get(), 0);
}

SimpleBuffer::~SimpleBuffer() {
  flush();
  unmap();
}

void* SimpleBuffer::map() {
  if( mMapped ) return nullptr;
  return Registrar::singleton().mapMemory(mDeviceMemory.get(), 0, VK_WHOLE_SIZE);
}

void SimpleBuffer::unmap() {
  if( !mMapped ) return;
  Registrar::singleton().unmapMemory(mDeviceMemory.get());
}

void SimpleBuffer::flush() {
  Registrar::singleton().flushMemoryRanges({vk::MappedMemoryRange(mDeviceMemory.get(), 0, VK_WHOLE_SIZE)});
}
