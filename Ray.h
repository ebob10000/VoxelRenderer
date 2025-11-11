#pragma once
#include <glm/glm.hpp>
#include <optional>

class World;

struct RaycastResult {
    glm::ivec3 blockPosition;
    glm::ivec3 faceNormal;
};

std::optional<RaycastResult> raycast(const glm::vec3& origin, const glm::vec3& direction, World& world, float maxDistance);