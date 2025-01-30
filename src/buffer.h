#pragma once
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>


struct Buffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;

    static Buffer create(size_t allocSize, VkBufferUsageFlags usageFlags, VmaMemoryUsage memoryUsage, VmaAllocator& allocator);
    void destroy(VmaAllocator& allocator);
};