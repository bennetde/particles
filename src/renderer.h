#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "swapchain.h"
#include "deletion_queue.h"
#include <optional>
#include "image.h"
#include "glm/glm.hpp"
#include "buffer.h"
#include "particles/particle.h"

constexpr unsigned int FRAME_OVERLAP = 1;

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;
    VkFence renderFence;
    VkSemaphore swapchainSemaphore;
    VkSemaphore renderSemaphore;
};

class ParticleRenderer {
public:
    void init();
    void run();
    void cleanup();
private:
    bool _isInitialized{false};

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkPhysicalDevice _physicalDevice;
    VkDevice _device;
    VkSurfaceKHR _surface;
    VmaAllocator _allocator;
    
    uint32_t _frameNumber{0};

    VkExtent2D _windowExtent{1700, 900};
    SDL_Window* _window;
    bool _stopRendering{false};

    std::optional<Swapchain> _swapchain;
    FrameData _frames[FRAME_OVERLAP];

    Image _renderImage;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    DeletionQueue _deletionQueue;    

    VkDescriptorPool _imguiDescriptorPool;
    VkSampler _renderImageImguiSampler;
    VkDescriptorSet _renderImageImguiTextureDescriptor;

    VkPipelineLayout _particlePipelineLayout;
    VkPipeline _particlePipeline;
    VkDescriptorPool _particlePool;
    VkDescriptorSetLayout _particlePipelineDescriptorSetLayout;


    glm::vec3 _cameraPosition = {0.0f, 0.0f, 0.0f};
    float _zoom = 0.1f;

    std::vector<Particle> _particles;
    uint32_t _particleCount = 500;
    Buffer _particleBuffer;
    Buffer _particleStagingBuffer;
    VkDeviceAddress _particleBufferAddress;
    bool _simulate = false;
    //std::array<std::array<int, 8>, 8> _attractions;
    float _attractions[8][8];


    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    void draw();
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawParticles(VkCommandBuffer cmd);
    void initVulkan();
    void initSwapchain();
    void initCommands();
    void initSyncStructures();
    void initImgui();
    void initParticlePipeline();
    void initParticles();
    void updateParticles();

    FrameData& getCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
};