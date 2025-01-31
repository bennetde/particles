#include "buffer.h"
#include <vma/vk_mem_alloc.h>
#include <stdexcept>

Buffer Buffer::create(size_t allocSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage, VmaAllocator &allocator) {
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usageFlags;

    VmaAllocationCreateInfo vmaAllocInfo = {};
    vmaAllocInfo.usage = memoryUsage;
    vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    if(memoryUsage == VMA_MEMORY_USAGE_AUTO) {
        vmaAllocInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }
    
    Buffer newBuffer;

    if(vmaCreateBuffer(allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.allocationInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    return newBuffer;
}

void Buffer::destroy(VmaAllocator& allocator) {
    vmaDestroyBuffer(allocator, buffer, allocation);
}