#include "Ray.h"
#include "World.h"
#include "Block.h"
#include <cmath>
#include <cfloat>
#include <glm/gtc/matrix_transform.hpp>

std::optional<RaycastResult> raycast(const glm::vec3& origin, const glm::vec3& direction, World& world, float maxDistance) {
    if (glm::length(direction) == 0.0f) return std::nullopt;

    glm::vec3 dir = glm::normalize(direction);
    glm::ivec3 currentVoxel = glm::floor(origin);

    glm::ivec3 step;
    step.x = (dir.x >= 0) ? 1 : -1;
    step.y = (dir.y >= 0) ? 1 : -1;
    step.z = (dir.z >= 0) ? 1 : -1;

    glm::vec3 nextVoxelBoundary(
        currentVoxel.x + (step.x > 0 ? 1.0f : 0.0f),
        currentVoxel.y + (step.y > 0 ? 1.0f : 0.0f),
        currentVoxel.z + (step.z > 0 ? 1.0f : 0.0f)
    );

    glm::vec3 tMax;
    tMax.x = (dir.x != 0) ? (nextVoxelBoundary.x - origin.x) / dir.x : FLT_MAX;
    tMax.y = (dir.y != 0) ? (nextVoxelBoundary.y - origin.y) / dir.y : FLT_MAX;
    tMax.z = (dir.z != 0) ? (nextVoxelBoundary.z - origin.z) / dir.z : FLT_MAX;

    glm::vec3 tDelta;
    tDelta.x = (dir.x != 0) ? static_cast<float>(step.x) / dir.x : FLT_MAX;
    tDelta.y = (dir.y != 0) ? static_cast<float>(step.y) / dir.y : FLT_MAX;
    tDelta.z = (dir.z != 0) ? static_cast<float>(step.z) / dir.z : FLT_MAX;

    float distance = 0.0f;
    glm::ivec3 lastVoxel = currentVoxel;

    while (distance < maxDistance) {
        if (world.getBlock(currentVoxel.x, currentVoxel.y, currentVoxel.z) != static_cast<unsigned char>(BlockID::Air)) {
            glm::ivec3 faceNormal(0);
            if (lastVoxel.x != currentVoxel.x) faceNormal.x = -step.x;
            else if (lastVoxel.y != currentVoxel.y) faceNormal.y = -step.y;
            else faceNormal.z = -step.z;
            return RaycastResult{ currentVoxel, faceNormal };
        }

        lastVoxel = currentVoxel;

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                currentVoxel.x += step.x;
                distance = tMax.x;
                tMax.x += tDelta.x;
            }
            else {
                currentVoxel.z += step.z;
                distance = tMax.z;
                tMax.z += tDelta.z;
            }
        }
        else {
            if (tMax.y < tMax.z) {
                currentVoxel.y += step.y;
                distance = tMax.y;
                tMax.y += tDelta.y;
            }
            else {
                currentVoxel.z += step.z;
                distance = tMax.z;
                tMax.z += tDelta.z;
            }
        }
    }
    return std::nullopt;
}