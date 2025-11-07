#include "World.h"
#include <iostream>

World::World() {
}

// addChunk now ONLY creates the chunk and adds it to the map.
// It does NOT generate the mesh.
void World::addChunk(int x, int y, int z) {
    glm::ivec3 pos(x, y, z);
    m_Chunks[pos] = std::make_unique<Chunk>(x, y, z);
    std::cout << "Added chunk data at: " << x << ", " << y << ", " << z << std::endl;
}

// This new function iterates through all existing chunks and builds their meshes.
void World::buildMeshes() {
    std::cout << "--- Building all chunk meshes ---" << std::endl;
    for (auto const& [pos, chunk] : m_Chunks) {
        chunk->generateMesh(*this);
        chunk->uploadMesh();
    }
    std::cout << "--- Finished building chunk meshes ---" << std::endl;
}

void World::render(Shader& shader) {
    for (auto const& [pos, chunk] : m_Chunks) {
        chunk->draw();
    }
}

unsigned char World::getBlock(int x, int y, int z) {
    int chunkX = floor((float)x / CHUNK_WIDTH);
    int chunkY = floor((float)y / CHUNK_HEIGHT);
    int chunkZ = floor((float)z / CHUNK_DEPTH);

    auto it = m_Chunks.find({ chunkX, chunkY, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return it->second->getBlock(localX, localY, localZ);
    }
    return 0;
}