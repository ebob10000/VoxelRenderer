#pragma once
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "Shader.h"

struct ivec3_comp {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

class World {
public:
    World();
    void addChunk(int x, int y, int z);

    // New function to build all meshes after chunks are added
    void buildMeshes();

    void render(Shader& shader);
    unsigned char getBlock(int x, int y, int z);

private:
    std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_comp> m_Chunks;
};