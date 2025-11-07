#pragma once
#include <map>
#include <memory>
#include <set>
#include <glm/glm.hpp>
#include "Chunk.h"
#include "Shader.h"
#include "TerrainGenerator.h"

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

    // The main update function, called every frame
    void update(const glm::vec3& playerPosition);

    void render(Shader& shader);
    unsigned char getBlock(int x, int y, int z);

    size_t getChunkCount() const;

private:
    void loadChunks(const glm::ivec3& playerChunkPos);
    void unloadChunks(const glm::ivec3& playerChunkPos);

    std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_comp> m_Chunks;
    std::unique_ptr<TerrainGenerator> m_TerrainGenerator;

    glm::ivec3 m_LastPlayerChunkPos;
    int m_RenderDistance = 8;
};