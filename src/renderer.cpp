#include "renderer.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <fmt/format.h>
#include <VkBootstrap.h>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#include "init.h"
#include "image.h"
#include "check.h"
#include "pipeline.h"
#include "push_constants/view_projection.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include "particles/particle.h"
#include <array>
#include <random>
#include <cstdlib>

constexpr bool useValidationLayers = true;

void ParticleRenderer::init() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
    _window = SDL_CreateWindow("Particle Life", _windowExtent.width, _windowExtent.height, windowFlags);

    if(_window == nullptr) {
        throw std::runtime_error(fmt::format("{}", SDL_GetError()));
    }

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();
    initImgui();
    initParticlePipeline();
    initParticles();
    _isInitialized = true;
}

void ParticleRenderer::run() {
    SDL_Event e;
    bool shouldQuit = false;

    if(!_isInitialized) {
        throw std::runtime_error("Cannot run renderer without being intitialized first.");
    }

    while(!shouldQuit) {
        while(SDL_PollEvent(&e) != 0) {
            if(e.type == SDL_EVENT_QUIT) {
                shouldQuit = true;
            }

            if(e.type == SDL_EVENT_WINDOW_MINIMIZED) {
                _stopRendering = true;
            } else if(e.type == SDL_EVENT_WINDOW_MAXIMIZED) {
                _stopRendering = false;
            }

            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        if(_stopRendering) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }


        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        // Render UI 
        {
            auto io = ImGui::GetIO();
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::SetNextWindowPos({0,0});
            if(ImGui::Begin("Main", (bool*)0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
                {
                    ImGui::BeginChild("Left Pane", ImVec2{150, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
                    ImGui::DragFloat3("Camera Position", (float*)&_cameraPosition);
                    ImGui::DragFloat("Camera Zoom", &_zoom, 0.001f, 0.01f, 5.0f);
                    ImGui::Checkbox("Simulate", &_simulate);
                    // Draw Attractions
                    ImGui::Text("Attraction Matrix:");
                    int index = 0;
                    for(int i = 0; i < 8; i++) {
                        for(int j = 0; j < 8; j++) {
                            ImGui::PushItemWidth(50.f);
                            ImGui::DragFloat(std::format("##{}", index).c_str(), &_attractions[i][j], 0.01f, -1.0f, 1.0f, "%.2f");
                            ImGui::SameLine();
                            index++;
                        }
                        ImGui::NewLine();
                        index++;
                    }
                    ImGui::EndChild();
                }
                ImGui::SameLine();
                {
                    ImGui::BeginChild("right pane");
                    ImGui::Image((ImTextureID)_renderImageImguiTextureDescriptor, ImVec2{800.0f, 800.0f});
                    ImGui::EndChild();
                }
            }
            ImGui::End();
        }

        updateParticles();
        ImGui::Render();
        draw();
    }
}

void ParticleRenderer::draw() {
    VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame().renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame().renderFence));

    uint32_t swapchainImageIndex;
    auto swapchain = &_swapchain.value();
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain->swapchain, 1000000000, getCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    // Begin Command Buffer for drawing
    VkCommandBufferBeginInfo cmdBeginInfo = init::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Clear current swapchain
    image::transitionImage(cmd, swapchain->images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);    
    image::transitionImage(cmd, _renderImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    VkClearColorValue clearValue;
    clearValue = { { 0.0f, 0.0f, 1.0f, 1.0f } };

    VkClearColorValue renderClear;
    renderClear = { { 0.0f, 0.0f, 0.0f, 1.0f }};

    VkImageSubresourceRange clearRange = init::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    vkCmdClearColorImage(cmd, swapchain->images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    vkCmdClearColorImage(cmd, _renderImage.image, VK_IMAGE_LAYOUT_GENERAL, &renderClear, 1, &clearRange);

    // Render particles
    image::transitionImage(cmd, _renderImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    drawParticles(cmd);

    // Draw Imgui onto swapchain
    image::transitionImage(cmd, _renderImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    image::transitionImage(cmd, swapchain->images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    drawImgui(cmd, swapchain->imageViews[swapchainImageIndex]);

    // Prepare swapchain for presentation
    image::transitionImage(cmd, swapchain->images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = init::CommandBufferSubmitInfo(cmd);
    VkSemaphoreSubmitInfo waitInfo = init::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = init::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submit = init::SubmitInfo(&cmdInfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, getCurrentFrame().renderFence));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));
    _frameNumber++;

}

void ParticleRenderer::drawParticles(VkCommandBuffer cmd) {
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;
    colorAttachment.imageView = _renderImage.view;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.renderArea = VkRect2D { VkOffset2D {0,0}, VkExtent2D{_renderImage.extent.width, _renderImage.extent.height} };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _particlePipeline);

    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _renderImage.extent.width;
    viewport.height = _renderImage.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = _renderImage.extent.width;
    scissor.extent.height = _renderImage.extent.height;

    glm::mat4 view = glm::translate(glm::mat4(1.f), _cameraPosition);
    glm::mat4 proj = glm::ortho(-1.0f / _zoom, 1.0f / _zoom, -1.0f / _zoom, 1.0f / _zoom, -100.0f, 100.0f);

    glm::mat4 viewProj = proj * view;

    ViewProjectionPushConstant viewProjPushConstant;
    viewProjPushConstant.viewProj = viewProj;
    viewProjPushConstant.particleBuffer = _particleBufferAddress;
    vkCmdPushConstants(cmd, _particlePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ViewProjectionPushConstant), &viewProjPushConstant);

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdDraw(cmd, 3, _particleCount, 0, 0);
    vkCmdEndRendering(cmd);
}

void ParticleRenderer::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.pNext = nullptr;
    colorAttachment.imageView = targetImageView;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; 

    VkRenderingInfo renderInfo = {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.renderArea = VkRect2D { VkOffset2D {0,0}, _swapchain.value().extent};
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.layerCount = 1;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

void ParticleRenderer::cleanup() {
    if(_isInitialized) {
        vkDeviceWaitIdle(_device);

        vkDestroyFence(_device, _immFence, nullptr);
        for(int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);

            vkDestroyFence(_device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);
        }

        _deletionQueue.flush();

        _swapchain.value().destroy(_device);

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);
    }
}

void ParticleRenderer::initVulkan() {
    vkb::InstanceBuilder builder;

    auto builtInstance = builder.set_app_name("Particle Renderer")
        .request_validation_layers(useValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1,3,0)
        .build();

    if(!builtInstance.has_value()) {
        throw std::runtime_error("Could not create Vulkan instance.");
    }
    auto vkbInstance = builtInstance.value();
    _instance = vkbInstance.instance;
    _debugMessenger = vkbInstance.debug_messenger;

    if(!SDL_Vulkan_CreateSurface(_window, _instance, NULL, &_surface)) {
        throw std::runtime_error(fmt::format("{}", SDL_GetError()));
    }
    
    VkPhysicalDeviceVulkan13Features features13 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12 { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    VkPhysicalDeviceFeatures features {};
    features.fillModeNonSolid = true;

    vkb::PhysicalDeviceSelector deviceSelector { vkbInstance };
    auto selectedPhysicalDevice = deviceSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_required_features(features)
        .set_surface(_surface)
        .select();

    if(!selectedPhysicalDevice.has_value()) {
        throw std::runtime_error("Failed to select physical device.");
    }

    vkb::DeviceBuilder deviceBuilder{ selectedPhysicalDevice.value() };
    auto builtDevice = deviceBuilder.build();

    if(!builtDevice.has_value()) {
        throw std::runtime_error("Failed to build device.");
    }

    _physicalDevice = selectedPhysicalDevice.value().physical_device;
    _device = builtDevice.value().device;

    _graphicsQueue = builtDevice.value().get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = builtDevice.value().get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &_allocator));

    _deletionQueue.pushFunction([&]() {
        vmaDestroyAllocator(_allocator);
    });
}

void ParticleRenderer::initSwapchain() {
    _swapchain = Swapchain{_physicalDevice, _device, _surface, _windowExtent.width, _windowExtent.height};

    VkExtent3D renderExtent = {1024, 1024, 1};
    VkImageUsageFlags renderImageUsage{};
    renderImageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    renderImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    renderImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    _renderImage = Image::create(_allocator, _device, renderExtent, VK_FORMAT_R16G16B16A16_SFLOAT, renderImageUsage, false);

    _deletionQueue.pushFunction([=]() {
        _renderImage.destroy(_device, _allocator);
    });
}

void ParticleRenderer::initCommands() {
    VkCommandPoolCreateInfo commandPoolInfo = init::CommandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i].commandPool));
        VkCommandBufferAllocateInfo cmdAllocInfo = init::CommandBufferAllocateInfo(_frames[i].commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i].mainCommandBuffer));
    }

    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_immCommandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = init::CommandBufferAllocateInfo(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_immCommandBuffer));

    _deletionQueue.pushFunction([=]() {
        vkDestroyCommandPool(_device, _immCommandPool, nullptr);
    });
}

void ParticleRenderer::initSyncStructures() {
    VkFenceCreateInfo fenceCreateInfo = init::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = init::SemaphoreCreateInfo();

    for(int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore));
    }

    VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immFence));
}

void ParticleRenderer::initImgui() {

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                   1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,    1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,             1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,      1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,            1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,    1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,          1000 }
    };

    VkDescriptorPoolCreateInfo imguiDescriptorPoolInfo = {};
    imguiDescriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imguiDescriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imguiDescriptorPoolInfo.maxSets = 100;
    imguiDescriptorPoolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(_device, &imguiDescriptorPoolInfo, nullptr, &_imguiDescriptorPool));

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForVulkan(_window);

    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = _instance;
    initInfo.PhysicalDevice = _physicalDevice;
    initInfo.Device = _device;
    initInfo.Queue = _graphicsQueue;
    initInfo.QueueFamily = _graphicsQueueFamily;
    initInfo.DescriptorPool = _imguiDescriptorPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchain.value().format;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    _deletionQueue.pushFunction([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(_device, _imguiDescriptorPool, nullptr);
    });


    VkSamplerCreateInfo renderImageSamplerInfo{};
    renderImageSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    renderImageSamplerInfo.magFilter = VK_FILTER_LINEAR;
    renderImageSamplerInfo.minFilter = VK_FILTER_LINEAR;
    renderImageSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    renderImageSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
    renderImageSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    renderImageSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    renderImageSamplerInfo.minLod = -1000;
    renderImageSamplerInfo.maxLod = 1000;
    renderImageSamplerInfo.maxAnisotropy = 1.0f;

    VK_CHECK(vkCreateSampler(_device, &renderImageSamplerInfo, nullptr, &_renderImageImguiSampler));
    _renderImageImguiTextureDescriptor = ImGui_ImplVulkan_AddTexture(_renderImageImguiSampler, _renderImage.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    _deletionQueue.pushFunction([=]() {
        vkDestroySampler(_device, _renderImageImguiSampler, nullptr);
    });
}

void ParticleRenderer::initParticlePipeline() {
    VkShaderModule particleFragShader;
    if(!init::LoadShaderModule("../shaders/particle.frag.spv", _device, &particleFragShader)) {
        throw std::runtime_error("Failed to load particle fragment shader");
    }

    VkShaderModule particleVertexShader;
    if(!init::LoadShaderModule("../shaders/particle.vert.spv", _device, &particleVertexShader)) {
        throw std::runtime_error("Failed to load particle vertex shader");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pNext = nullptr;
    pipelineLayoutInfo.flags = 0;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;

    // Set push constants
    VkPushConstantRange viewProjPushConstant;
    viewProjPushConstant.offset = 0;
    viewProjPushConstant.size = sizeof(ViewProjectionPushConstant);
    viewProjPushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &viewProjPushConstant;

    // Set buffers
    VkDescriptorSetLayoutBinding particlesLayoutBinding{};
    particlesLayoutBinding.binding = 0;
    particlesLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    particlesLayoutBinding.descriptorCount = 1;
    particlesLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo particleSetLayoutInfo = {};
    particleSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    particleSetLayoutInfo.pNext = nullptr;
    particleSetLayoutInfo.bindingCount = 1;
    particleSetLayoutInfo.pBindings = &particlesLayoutBinding; 

    VK_CHECK(vkCreateDescriptorSetLayout(_device, &particleSetLayoutInfo, nullptr, &_particlePipelineDescriptorSetLayout));

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &_particlePipelineDescriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_particlePipelineLayout));

    PipelineBuilder builder;
    builder._pipelineLayout = _particlePipelineLayout;
    builder.setShaders(particleVertexShader, particleFragShader);
    builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    builder.setMultisamplingNone();
    builder.disableBlending();
    builder.disableDepthtest();
    builder.setColorAttachmentFormat(_renderImage.format);
    builder.setDepthFormat(VK_FORMAT_UNDEFINED);
    
    _particlePipeline = builder.build(_device);

    vkDestroyShaderModule(_device, particleFragShader, nullptr);
    vkDestroyShaderModule(_device, particleVertexShader, nullptr);

    _deletionQueue.pushFunction([&]() {
        vkDestroyDescriptorSetLayout(_device, _particlePipelineDescriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(_device, _particlePipelineLayout, nullptr);
        vkDestroyPipeline(_device, _particlePipeline, nullptr);
    });
}

void ParticleRenderer::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(_device, 1, &_immFence));
	VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

	VkCommandBuffer cmd = _immCommandBuffer;

	VkCommandBufferBeginInfo cmdBeginInfo = init::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdinfo = init::CommandBufferSubmitInfo(cmd);
	VkSubmitInfo2 submit = init::SubmitInfo(&cmdinfo, nullptr, nullptr);

	// submit command buffer to the queue and execute it.
	//  _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immFence));

	VK_CHECK(vkWaitForFences(_device, 1, &_immFence, true, 9999999999));
}

void ParticleRenderer::initParticles() {
    std::srand(std::time(nullptr));
    _particleBuffer = Buffer::create(sizeof(Particle) * _particleCount, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY, _allocator);
    _particleStagingBuffer = Buffer::create(sizeof(Particle) * _particleCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, _allocator);

    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            float r = (static_cast <float> (rand()) / static_cast <float> (RAND_MAX)) * 2 - 1;
            _attractions[i][j] = r;
        }
    }

    _particles.reserve(_particleCount);

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<> dis(-10.0f, 10.0f);
    std::uniform_int_distribution<> colorDistribution(0, 7);
    for(int i = 0; i < _particleCount; i++) {
        _particles.push_back({ .position = {dis(gen), dis(gen), 0.0f, 0.0f}, .velocity = glm::vec4{0.0f}, .color = glm::ivec4{colorDistribution(gen)}});
    }

    void* data = _particleStagingBuffer.allocation->GetMappedData();
    memcpy(data, _particles.data(), sizeof(Particle) * _particleCount);

    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copy{0};
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = sizeof(Particle) * _particleCount;

        vkCmdCopyBuffer(cmd, _particleStagingBuffer.buffer, _particleBuffer.buffer, 1, &copy);
    });

    VkBufferDeviceAddressInfo deviceAddressInfo { .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = _particleBuffer.buffer};
    _particleBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);


    _deletionQueue.pushFunction([&]() {
        _particleStagingBuffer.destroy(_allocator);
        _particleBuffer.destroy(_allocator);
    });
}

void ParticleRenderer::updateParticles() {
    if(!_simulate) return;
    float dt = 0.02f;

    for(int i = 0; i < _particles.size(); i++) {

        _particles[i].velocity *= pow(0.5f, dt / 0.04f);
        glm::vec2 force = glm::vec2{0.0f};

        for(int j = 0; j < _particles.size(); j++) {
            if(i == j) continue;

            float a = _attractions[_particles[i].color.r][_particles[j].color.r];
            glm::vec2 forceChange = _particles[i].computeForce(_particles[j], a);
            if(force.x == NAN || force.y == NAN) continue; 
            force -= forceChange; 
        }
        _particles[i].velocity += glm::vec4(force, 0.0f, 0.0f) * dt;
        _particles[i].position += _particles[i].velocity * dt;
    }


    void* data = _particleStagingBuffer.allocation->GetMappedData();
    memcpy(data, _particles.data(), sizeof(Particle) * _particleCount);

    immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copy{0};
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = sizeof(Particle) * _particleCount;

        vkCmdCopyBuffer(cmd, _particleStagingBuffer.buffer, _particleBuffer.buffer, 1, &copy);
    });
}
