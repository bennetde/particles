#pragma once
#include "vulkan/vulkan.h"

struct Swapchain {
    VkFormat format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    Swapchain(VkPhysicalDevice &physicalDevice, VkDevice &device, VkSurfaceKHR &surface, uint32_t width, uint32_t height);
    void destroy(VkDevice &device);
};