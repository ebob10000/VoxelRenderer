#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include "Block.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>

const float ATLAS_WIDTH_TILES = 16.0f;
const float ATLAS_HEIGHT_TILES = 16.0f;
const float TILE_WIDTH_NORMALIZED = 1.0f / ATLAS_WIDTH_TILES;
const float TILE_HEIGHT_NORMALIZED = 1.0f / ATLAS_HEIGHT_TILES;

ChunkMeshingData::ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos) {
    m_LeafQuality = world.m_LeafQuality.load();
    std::array<std::shared_ptr<const Chunk>, 9> neighbors{};
    {
        std::shared_lock<std::shared_mutex> lock(world.m_ChunksMutex);
        int i = 0;
        for (int z = -1; z <= 1; ++z) {
            for (int x = -1; x <= 1; ++x) {
                glm::ivec3 pos = centralChunkPos + glm::ivec3(x, 0, z);
                auto it = world.m_Chunks.find(pos);
                if (it != world.m_Chunks.end()) {
                    neighbors[i] = it->second;
                }
                i++;
            }
        }
    }

    if (neighbors[4]) { // Center chunk
        const auto& center_chunk = neighbors[4];
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    m_Blocks[x + 1][y][z + 1] = center_chunk->getBlock(x, y, z);
                    unsigned char sun = center_chunk->getSunlight(x, y, z);
                    unsigned char block = center_chunk->getBlockLight(x, y, z);
                    m_LightLevels[x + 1][y][z + 1] = (sun << 4) | block;
                }
            }
        }
    }


    auto getNeighborLight = [&](const std::shared_ptr<const Chunk>& neighbor, int x, int y, int z) -> unsigned char {
        if (!neighbor) return (15 << 4); // Full sun if no chunk
        unsigned char sun = neighbor->getSunlight(x, y, z);
        unsigned char block = neighbor->getBlockLight(x, y, z);
        return (sun << 4) | block;
        };

    if (neighbors[3]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[0][y][z + 1] = neighbors[3]->getBlock(CHUNK_WIDTH - 1, y, z);
            m_LightLevels[0][y][z + 1] = getNeighborLight(neighbors[3], CHUNK_WIDTH - 1, y, z);
        }
    }
    if (neighbors[5]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[CHUNK_WIDTH + 1][y][z + 1] = neighbors[5]->getBlock(0, y, z);
            m_LightLevels[CHUNK_WIDTH + 1][y][z + 1] = getNeighborLight(neighbors[5], 0, y, z);
        }
    }
    if (neighbors[1]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][0] = neighbors[1]->getBlock(x, y, CHUNK_DEPTH - 1);
            m_LightLevels[x + 1][y][0] = getNeighborLight(neighbors[1], x, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[7]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][CHUNK_DEPTH + 1] = neighbors[7]->getBlock(x, y, 0);
            m_LightLevels[x + 1][y][CHUNK_DEPTH + 1] = getNeighborLight(neighbors[7], x, y, 0);
        }
    }
    if (neighbors[0]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[0][y][0] = neighbors[0]->getBlock(CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1);
            m_LightLevels[0][y][0] = getNeighborLight(neighbors[0], CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[2]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[CHUNK_WIDTH + 1][y][0] = neighbors[2]->getBlock(0, y, CHUNK_DEPTH - 1);
            m_LightLevels[CHUNK_WIDTH + 1][y][0] = getNeighborLight(neighbors[2], 0, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[6]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[0][y][CHUNK_DEPTH + 1] = neighbors[6]->getBlock(CHUNK_WIDTH - 1, y, 0);
            m_LightLevels[0][y][CHUNK_DEPTH + 1] = getNeighborLight(neighbors[6], CHUNK_WIDTH - 1, y, 0);
        }
    }
    if (neighbors[8]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[CHUNK_WIDTH + 1][y][CHUNK_DEPTH + 1] = neighbors[8]->getBlock(0, y, 0);
            m_LightLevels[CHUNK_WIDTH + 1][y][CHUNK_DEPTH + 1] = getNeighborLight(neighbors[8], 0, y, 0);
        }
    }
}

unsigned char ChunkMeshingData::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    return m_Blocks[x + 1][y][z + 1];
}

unsigned char ChunkMeshingData::getSunlight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 15;
    return m_LightLevels[x + 1][y][z + 1] >> 4;
}

unsigned char ChunkMeshingData::getBlockLight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    return m_LightLevels[x + 1][y][z + 1] & 0x0F;
}

namespace {
    float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) {
            return 3.0f;
        }
        return static_cast<float>(side1 + side2 + corner);
    }
}

void SimpleMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& opaqueMesh, Mesh& transparentMesh, bool smoothLighting) {
    opaqueMesh.vertices.clear();
    opaqueMesh.indices.clear();
    transparentMesh.vertices.clear();
    transparentMesh.indices.clear();
    unsigned int opaqueVertexCount = 0;
    unsigned int transparentVertexCount = 0;

    float chunkWorldX = static_cast<float>(chunkPosition.x * CHUNK_WIDTH);
    float chunkWorldY = static_cast<float>(chunkPosition.y * CHUNK_HEIGHT);
    float chunkWorldZ = static_cast<float>(chunkPosition.z * CHUNK_DEPTH);

    auto addFace = [&](int x, int y, int z, int faceIndex, Mesh& mesh, unsigned int& vertexCount) {
        BlockID blockID = (BlockID)data.getBlock(x, y, z);
        const BlockData& blockData = BlockDataManager::getData(blockID);
        glm::ivec2 texCoords = blockData.faces[faceIndex].tex_coords;

        float u_min = texCoords.x * TILE_WIDTH_NORMALIZED;
        float v_min = texCoords.y * TILE_HEIGHT_NORMALIZED;

        int nx = x + faceNormals[faceIndex][0];
        int ny = y + faceNormals[faceIndex][1];
        int nz = z + faceNormals[faceIndex][2];

        float ao[4];

        for (int i = 0; i < 4; i++) {
            int vIndex = (faceIndex * 4 + i);

            float local_u = faceVertices[vIndex * 5 + 3];
            float local_v = faceVertices[vIndex * 5 + 4];
            float atlas_u = u_min + local_u * TILE_WIDTH_NORMALIZED;
            float atlas_v = v_min + local_v * TILE_HEIGHT_NORMALIZED;

            mesh.vertices.insert(mesh.vertices.end(), {
                faceVertices[vIndex * 5 + 0] + chunkWorldX + x + 0.5f,
                faceVertices[vIndex * 5 + 1] + chunkWorldY + y + 0.5f,
                faceVertices[vIndex * 5 + 2] + chunkWorldZ + z + 0.5f,
                atlas_u,
                atlas_v
                });

            if (smoothLighting) {
                bool s1 = !BlockDataManager::isTransparentForLighting((BlockID)data.getBlock(x + aoCheck[faceIndex][i][0][0], y + aoCheck[faceIndex][i][0][1], z + aoCheck[faceIndex][i][0][2]));
                bool s2 = !BlockDataManager::isTransparentForLighting((BlockID)data.getBlock(x + aoCheck[faceIndex][i][1][0], y + aoCheck[faceIndex][i][1][1], z + aoCheck[faceIndex][i][1][2]));
                bool c = !BlockDataManager::isTransparentForLighting((BlockID)data.getBlock(x + aoCheck[faceIndex][i][2][0], y + aoCheck[faceIndex][i][2][1], z + aoCheck[faceIndex][i][2][2]));
                ao[i] = calculateAO(s1, s2, c);
            }
            else {
                ao[i] = 0.0f;
            }
            mesh.vertices.push_back(ao[i]);

            if (smoothLighting) {
                glm::ivec3 n_main = { nx, ny, nz };
                glm::ivec3 n_side1 = { x + aoCheck[faceIndex][i][0][0], y + aoCheck[faceIndex][i][0][1], z + aoCheck[faceIndex][i][0][2] };
                glm::ivec3 n_side2 = { x + aoCheck[faceIndex][i][1][0], y + aoCheck[faceIndex][i][1][1], z + aoCheck[faceIndex][i][1][2] };
                glm::ivec3 n_corner = { x + aoCheck[faceIndex][i][2][0], y + aoCheck[faceIndex][i][2][1], z + aoCheck[faceIndex][i][2][2] };

                float sun_total = 0;
                float block_total = 0;

                sun_total += data.getSunlight(n_main.x, n_main.y, n_main.z);
                block_total += data.getBlockLight(n_main.x, n_main.y, n_main.z);
                sun_total += data.getSunlight(n_side1.x, n_side1.y, n_side1.z);
                block_total += data.getBlockLight(n_side1.x, n_side1.y, n_side1.z);
                sun_total += data.getSunlight(n_side2.x, n_side2.y, n_side2.z);
                block_total += data.getBlockLight(n_side2.x, n_side2.y, n_side2.z);
                sun_total += data.getSunlight(n_corner.x, n_corner.y, n_corner.z);
                block_total += data.getBlockLight(n_corner.x, n_corner.y, n_corner.z);

                float final_light = std::max(sun_total / 4.0f, block_total / 4.0f);
                if (blockData.emissionStrength > 0) {
                    final_light = static_cast<float>(blockData.emissionStrength);
                }
                mesh.vertices.push_back(final_light);
            }
            else {
                float face_sunlight = static_cast<float>(data.getSunlight(nx, ny, nz));
                float face_blocklight = static_cast<float>(data.getBlockLight(nx, ny, nz));
                float lightLevel = std::max(face_sunlight, face_blocklight);
                if (blockData.emissionStrength > 0) {
                    lightLevel = static_cast<float>(blockData.emissionStrength);
                }
                mesh.vertices.push_back(lightLevel);
            }
            mesh.vertices.push_back(static_cast<float>(faceIndex));
        }

        if (smoothLighting && (ao[0] + ao[2] > ao[1] + ao[3])) {
            mesh.indices.insert(mesh.indices.end(), { vertexCount, vertexCount + 1, vertexCount + 3, vertexCount + 1, vertexCount + 2, vertexCount + 3 });
        }
        else {
            mesh.indices.insert(mesh.indices.end(), { vertexCount, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount });
        }
        vertexCount += 4;
        };

    LeafQuality quality = data.getLeafQuality();

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                BlockID currentBlock = (BlockID)data.getBlock(x, y, z);
                if (currentBlock == BlockID::Air) continue;

                auto checkFace = [&](int nx, int ny, int nz, int faceIndex) {
                    BlockID neighborBlock = (BlockID)data.getBlock(nx, ny, nz);
                    bool shouldDraw = false;

                    if (currentBlock == BlockID::OakLeaves && neighborBlock == BlockID::OakLeaves && quality == LeafQuality::Fancy) {
                        if (faceIndex == 1 || faceIndex == 3 || faceIndex == 5) { // Positive faces
                            shouldDraw = true;
                        }
                    }
                    else {
                        shouldDraw = BlockDataManager::shouldRenderFace(currentBlock, neighborBlock, quality);
                    }

                    if (shouldDraw) {
                        bool isTransparentPass = (currentBlock == BlockID::OakLeaves && quality != LeafQuality::Fast);
                        if (isTransparentPass) {
                            addFace(x, y, z, faceIndex, transparentMesh, transparentVertexCount);
                        }
                        else {
                            addFace(x, y, z, faceIndex, opaqueMesh, opaqueVertexCount);
                        }
                    }
                    };

                checkFace(x + 1, y, z, 1);
                checkFace(x - 1, y, z, 0);
                checkFace(x, y + 1, z, 3);
                checkFace(x, y - 1, z, 2);
                checkFace(x, y, z + 1, 5);
                checkFace(x, y, z - 1, 4);
            }
        }
    }
}

void GreedyMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& opaqueMesh, Mesh& transparentMesh, bool smoothLighting) {
    SimpleMesher simple;
    simple.generateMesh(data, chunkPosition, opaqueMesh, transparentMesh, smoothLighting);
}