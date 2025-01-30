#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

namespace image {
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentImage, VkImageLayout newLayout);
}

struct Image {
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VkExtent3D extent;
    VkFormat format;

    
    static Image create(VmaAllocator& allocator, VkDevice device, VkExtent3D size, VkFormat format, VkImageUsageFlags useFlags, bool mipmapped = false);
    void destroy(VkDevice device, VmaAllocator &allocator);
};