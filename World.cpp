#include "World.h"
#include <iostream>

World::World() : m_LastPlayerChunkPos(9999, 9999, 9999) { // Initialize to a far-away position
    m_TerrainGenerator = std::make_unique<TerrainGenerator>(1337);
}

void World::update(const glm::vec3& playerPosition) {
    // Convert player's world position to their current chunk coordinate
    glm::ivec3 playerChunkPos(
        floor(playerPosition.x / CHUNK_WIDTH),
        0, // We are only doing a 2D grid of chunks for now
        floor(playerPosition.z / CHUNK_DEPTH)
    );

    // Only do work if the player has crossed into a new chunk
    if (playerChunkPos != m_LastPlayerChunkPos) {
        loadChunks(playerChunkPos);
        unloadChunks(playerChunkPos);
        m_LastPlayerChunkPos = playerChunkPos;
    }
}

void World::loadChunks(const glm::ivec3& playerChunkPos) {
    std::set<glm::ivec3, ivec3_comp> chunksToRemesh;

    // Explicitly cast to int to remove warnings
    for (int x = playerChunkPos.x - m_RenderDistance; x <= playerChunkPos.x + m_RenderDistance; ++x) {
        for (int z = playerChunkPos.z - m_RenderDistance; z <= playerChunkPos.z + m_RenderDistance; ++z) {
            glm::ivec3 chunkPos(x, 0, z);

            if (m_Chunks.find(chunkPos) == m_Chunks.end()) {
                auto newChunk = std::make_unique<Chunk>(chunkPos.x, chunkPos.y, chunkPos.z);
                m_TerrainGenerator->generateChunkData(*newChunk);
                m_Chunks[chunkPos] = std::move(newChunk);
                std::cout << "Created chunk at: " << chunkPos.x << ", " << chunkPos.z << std::endl;

                chunksToRemesh.insert(chunkPos);
                chunksToRemesh.insert({ chunkPos.x + 1, 0, chunkPos.z });
                chunksToRemesh.insert({ chunkPos.x - 1, 0, chunkPos.z });
                chunksToRemesh.insert({ chunkPos.x, 0, chunkPos.z + 1 });
                chunksToRemesh.insert({ chunkPos.x, 0, chunkPos.z - 1 });
            }
        }
    }

    for (const auto& pos : chunksToRemesh) {
        auto it = m_Chunks.find(pos);
        if (it != m_Chunks.end()) {
            it->second->generateMesh(*this);
            it->second->uploadMesh();
        }
    }
}

void World::unloadChunks(const glm::ivec3& playerChunkPos) {
    // Create a list of chunks to remove
    std::vector<glm::ivec3> toRemove;
    for (auto const& [pos, chunk] : m_Chunks) {
        // If a chunk is too far from the player, mark it for removal
        if (abs(pos.x - playerChunkPos.x) > m_RenderDistance ||
            abs(pos.z - playerChunkPos.z) > m_RenderDistance) {
            toRemove.push_back(pos);
        }
    }

    // Remove the marked chunks from the map
    for (const auto& pos : toRemove) {
        m_Chunks.erase(pos);
        std::cout << "Unloaded chunk at: " << pos.x << ", " << pos.z << std::endl;
    }
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

size_t World::getChunkCount() const {
    return m_Chunks.size();
}