#pragma once
#include "Mesh.h"
#include <array>
#include <glm/glm.hpp>

class Chunk;
class World;

// Provides a fast, lock-free view of a 3x3x1 area of chunks for a meshing job.
class ChunkMeshingData {
public:
    ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos);
    unsigned char getBlock(int x, int y, int z) const;
    unsigned char getLight(int x, int y, int z) const;

private:
    // A 3x3 grid of chunk pointers centered on the chunk to be meshed
    std::array<const Chunk*, 9> m_ChunkNeighbors{};
};

class IMesher {
public:
    // The mesher now only needs the local data provider to do its job.
    virtual void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) = 0;
};

class SimpleMesher : public IMesher {
public:
    void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) override;
};

class GreedyMesher : public IMesher {
public:
    void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) override;
};