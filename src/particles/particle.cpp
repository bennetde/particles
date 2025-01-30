#include "particle.h"
#define GLM_SWIZZLE
#include <glm/glm.hpp>

const float BETA = 0.3f;

glm::vec2 Particle::getPosition() {
    return glm::vec2{position.x, position.y};
}

glm::vec2 Particle::computeForce(Particle& other, float a) {
    float dist = glm::distance(getPosition(), other.getPosition()) / R_MAX;
    float force = 0.0f;
    if(dist < BETA) {
        force = dist / BETA - 1.0f;
    } else if (BETA < dist && dist < 1.0f) {
        force = a * (1 - std::abs(2.0f * dist - 1.0f - BETA) / (1.0f - BETA)); 
    } else {
        force = 0;
    }

    glm::vec2 dir = glm::normalize(getPosition() - other.getPosition());
    return dir * force * R_MAX * FORCE_FACTOR;
}