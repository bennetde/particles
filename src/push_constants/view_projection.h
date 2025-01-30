#pragma once
#include <glm/glm.hpp>

struct ViewProjectionPushConstant{
    glm::mat4 viewProj;
    VkDeviceAddress particleBuffer;
};