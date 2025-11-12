#pragma once
#include "Mesh.h"
#include "Chunk.h"
#include <array>
#include <glm/glm.hpp>
#include <memory>

class Chunk;
class World;

const int PADDED_WIDTH = CHUNK_WIDTH + 2;
const int PADDED_HEIGHT = CHUNK_HEIGHT;
const int PADDED_DEPTH = CHUNK_DEPTH + 2;

class ChunkMeshingData {
public:
    ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos);

    unsigned char getBlock(int x, int y, int z) const;
    unsigned char getSunlight(int x, int y, int z) const;
    unsigned char getBlockLight(int x, int y, int z) const;

private:
    unsigned char m_Blocks[PADDED_WIDTH][PADDED_HEIGHT][PADDED_DEPTH] = { 0 };
    unsigned char m_LightLevels[PADDED_WIDTH][PADDED_HEIGHT][PADDED_DEPTH] = { 0 };
};

class IMesher {
public:
    virtual void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh, bool smoothLighting) = 0;
};

class SimpleMesher : public IMesher {
public:
    void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh, bool smoothLighting) override;
};

class GreedyMesher : public IMesher {
public:
    void generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh, bool smoothLighting) override;
};