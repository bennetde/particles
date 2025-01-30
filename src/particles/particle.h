#pragma once
#include <glm/glm.hpp>

const float R_MAX = 2.0f;
const float FORCE_FACTOR = 10.0f;

struct Particle {
    glm::vec4 position;
    glm::vec4 velocity;
    glm::ivec4 color;

    glm::vec2 getPosition();
    glm::vec2 computeForce(Particle& other, float a);
};