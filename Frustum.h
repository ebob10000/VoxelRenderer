#pragma once
#include <glm/glm.hpp>
#include <array>

class Frustum {
public:
    std::array<glm::vec4, 6> planes;

    void update(const glm::mat4& projViewMatrix) {
        const glm::mat4& m = projViewMatrix;

        // Left plane
        planes[0] = glm::vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]);
        // Right plane
        planes[1] = glm::vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]);
        // Bottom plane
        planes[2] = glm::vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]);
        // Top plane
        planes[3] = glm::vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]);
        // Near plane
        planes[4] = glm::vec4(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2], m[3][3] + m[3][2]);
        // Far plane
        planes[5] = glm::vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]);

        for (int i = 0; i < 6; i++) {
            planes[i] = glm::normalize(planes[i]);
        }
    }

    bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const {
        for (int i = 0; i < 6; i++) {
            glm::vec3 p = min;
            if (planes[i].x >= 0) p.x = max.x;
            if (planes[i].y >= 0) p.y = max.y;
            if (planes[i].z >= 0) p.z = max.z;

            if (glm::dot(glm::vec3(planes[i]), p) + planes[i].w < 0) {
                return false;
            }
        }
        return true;
    }
};