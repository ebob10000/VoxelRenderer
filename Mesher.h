#pragma once
#include "Mesh.h"
#include "Chunk.h"
#include <array>
#include <glm/glm.hpp>
#include <memory>

class Chunk;
class World;

// Padded dimensions for the local data buffer
const int PADDED_WIDTH = CHUNK_WIDTH + 2;
const int PADDED_HEIGHT = CHUNK_HEIGHT; // No padding needed on Y for this mesher
const int PADDED_DEPTH = CHUNK_DEPTH + 2;

// Provides a fast, lock-free view of a 3x3x1 area of chunks for a meshing job
// by pre-copying all required data into a local buffer.
class ChunkMeshingData {
public:
    ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos);

    // These functions now become incredibly fast array lookups.
    unsigned char getBlock(int x, int y, int z) const;
    unsigned char getLight(int x, int y, int z) const;

private:
    unsigned char m_Blocks[PADDED_WIDTH][PADDED_HEIGHT][PADDED_DEPTH] = { 0 };
    unsigned char m_LightLevels[PADDED_WIDTH][PADDED_HEIGHT][PADDED_DEPTH] = { 0 };
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