#pragma once
#include <vector>
#include <glm/glm.hpp>

struct MeshData {
    glm::ivec3 chunkPosition;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};