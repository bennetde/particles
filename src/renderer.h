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
#include "particles/simulation_settings.h"

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
    uint32_t _particleCount = 32768/2;
    glm::vec2 _spawnBoundaries = {-100.0f, 100.0f};
    Buffer _particleBuffer;
    Buffer _particleStagingBuffer;
    VkDeviceAddress _particleBufferAddress;
    bool _simulate = false;
    //std::array<std::array<int, 8>, 8> _attractions;
    SimulationSettings _simulationSettings;
    Buffer _simulationSettingsBuffer;

    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;


    VkDescriptorPool _computeDescriptorPool;
    VkDescriptorSetLayout _computeDescriptorSetLayout;
    VkDescriptorSet _computeDescriptorSet;
    VkDescriptorSet _simulationSettingsDescriptorSet;
    VkPipeline _computePipeline;
    VkPipelineLayout _computePipelineLayout;

    float _frametime;

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
    void initComputePipeline();

    FrameData& getCurrentFrame() { return _frames[_frameNumber % FRAME_OVERLAP]; };
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
};