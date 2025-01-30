#include "swapchain.h"
#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

Swapchain::Swapchain(VkPhysicalDevice& physicalDevice, VkDevice& device, VkSurfaceKHR& surface, uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder {physicalDevice, device, surface};

    format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format({ .format = format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    extent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;
    images = vkbSwapchain.get_images().value();
    imageViews = vkbSwapchain.get_image_views().value();
}

void Swapchain::destroy(VkDevice& device) {
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for(int i = 0; i < imageViews.size(); i++) {
        vkDestroyImageView(device, imageViews[i], nullptr);
    }
}