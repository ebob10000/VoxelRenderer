#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include "Block.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm> // For std::swap

const float ATLAS_WIDTH_TILES = 16.0f;
const float ATLAS_HEIGHT_TILES = 16.0f;
const float TILE_WIDTH_NORMALIZED = 1.0f / ATLAS_WIDTH_TILES;
const float TILE_HEIGHT_NORMALIZED = 1.0f / ATLAS_HEIGHT_TILES;

ChunkMeshingData::ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos) {
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

    std::memset(m_Blocks, 0, sizeof(m_Blocks));
    std::memset(m_LightLevels, 15, sizeof(m_LightLevels));

    if (neighbors[4]) { // Central chunk
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    m_Blocks[x + 1][y][z + 1] = neighbors[4]->blocks[x][y][z];
                    m_LightLevels[x + 1][y][z + 1] = neighbors[4]->lightLevels[x][y][z];
                }
            }
        }
    }

    // -X neighbor
    if (neighbors[3]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[0][y][z + 1] = neighbors[3]->blocks[CHUNK_WIDTH - 1][y][z];
            m_LightLevels[0][y][z + 1] = neighbors[3]->lightLevels[CHUNK_WIDTH - 1][y][z];
        }
    }
    // +X neighbor
    if (neighbors[5]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[CHUNK_WIDTH + 1][y][z + 1] = neighbors[5]->blocks[0][y][z];
            m_LightLevels[CHUNK_WIDTH + 1][y][z + 1] = neighbors[5]->lightLevels[0][y][z];
        }
    }
    // -Z neighbor
    if (neighbors[1]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][0] = neighbors[1]->blocks[x][y][CHUNK_DEPTH - 1];
            m_LightLevels[x + 1][y][0] = neighbors[1]->lightLevels[x][y][CHUNK_DEPTH - 1];
        }
    }
    // +Z neighbor
    if (neighbors[7]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][CHUNK_DEPTH + 1] = neighbors[7]->blocks[x][y][0];
            m_LightLevels[x + 1][y][CHUNK_DEPTH + 1] = neighbors[7]->lightLevels[x][y][0];
        }
    }
}

unsigned char ChunkMeshingData::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    return m_Blocks[x + 1][y][z + 1];
}

unsigned char ChunkMeshingData::getLight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 15;
    return m_LightLevels[x + 1][y][z + 1];
}

namespace {
    inline bool isOpaque(unsigned char blockID) {
        return blockID != 0 && blockID != (unsigned char)BlockID::Water;
    }

    inline float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 3.0f;
        return static_cast<float>(side1 + side2 + corner);
    }
}

void SimpleMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexCount = 0;

    const float TILE_WIDTH_NORMALIZED = 1.0f / ATLAS_WIDTH_TILES;
    const float TILE_HEIGHT_NORMALIZED = 1.0f / ATLAS_HEIGHT_TILES;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                unsigned char blockRaw = data.getBlock(x, y, z);
                if (blockRaw == 0) continue;

                BlockID blockID = static_cast<BlockID>(blockRaw);
                const BlockData& blockData = BlockDataManager::getData(blockID);

                for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
                    int nx = x + faceNormals[faceIndex][0];
                    int ny = y + faceNormals[faceIndex][1];
                    int nz = z + faceNormals[faceIndex][2];

                    unsigned char adjacentBlock = data.getBlock(nx, ny, nz);
                    if (isOpaque(blockRaw) == isOpaque(adjacentBlock)) {
                        continue;
                    }

                    float ao[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    float lightVal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

                    for (int vi = 0; vi < 4; ++vi) {
                        const int (*offsets)[3] = aoCheck[faceIndex][vi];
                        bool s1 = isOpaque(data.getBlock(x + offsets[0][0], y + offsets[0][1], z + offsets[0][2]));
                        bool s2 = isOpaque(data.getBlock(x + offsets[1][0], y + offsets[1][1], z + offsets[1][2]));
                        bool c = isOpaque(data.getBlock(x + offsets[2][0], y + offsets[2][1], z + offsets[2][2]));
                        ao[vi] = calculateAO(s1, s2, c);

                        glm::ivec3 light_pos = { x, y, z };
                        light_pos[faceIndex / 2] += (faceIndex % 2 == 0 ? -1 : 1);
                        lightVal[vi] = static_cast<float>(data.getLight(light_pos.x, light_pos.y, light_pos.z));
                    }

                    float face_verts[4][3];
                    for (int vi = 0; vi < 4; ++vi) {
                        const float* src = faceVertices + (faceIndex * 4 + vi) * 5;
                        face_verts[vi][0] = src[0];
                        face_verts[vi][1] = src[1];
                        face_verts[vi][2] = src[2];
                    }

                    float block_pos[3] = {
                        static_cast<float>(x) + 0.5f,
                        static_cast<float>(y) + 0.5f,
                        static_cast<float>(z) + 0.5f
                    };

                    if (ao[0] + ao[2] > ao[1] + ao[3]) {
                        indices.push_back(vertexCount);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 3);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 3);
                    }
                    else {
                        indices.push_back(vertexCount);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 3);
                        indices.push_back(vertexCount);
                    }
                    vertexCount += 4;
                }
            }
        }
    }

    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
}
void GreedyMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();

    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexCount = 0;

    const int sizes[3] = { CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH };

    for (int direction = 0; direction < 6; ++direction) {
        int normalAxis = direction / 2;
        int uAxis = (normalAxis + 1) % 3;
        int vAxis = (normalAxis + 2) % 3;
        int sign = (direction % 2 == 0) ? -1 : 1;

        int size_u = sizes[uAxis];
        int size_v = sizes[vAxis];
        int size_n = sizes[normalAxis];

        glm::ivec3 normalVec = { faceNormals[direction][0], faceNormals[direction][1], faceNormals[direction][2] };

        for (int w = 0; w < size_n; ++w) {
            std::vector<std::vector<char>> mask(size_u, std::vector<char>(size_v, 0));

            for (int j = 0; j < size_v; ++j) {
                for (int i = 0; i < size_u; ) {
                    int pos[3];
                    pos[uAxis] = i;
                    pos[vAxis] = j;
                    pos[normalAxis] = w;

                    int x = pos[0], y = pos[1], z = pos[2];
                    BlockID blockID = static_cast<BlockID>(data.getBlock(x, y, z));
                    if (blockID == BlockID::Air || mask[i][j]) {
                        ++i;
                        continue;
                    }

                    int adj[3] = { x + normalVec[0], y + normalVec[1], z + normalVec[2] };
                    unsigned char adjacentBlock = data.getBlock(adj[0], adj[1], adj[2]);
                    if (isOpaque((unsigned char)blockID) == isOpaque(adjacentBlock)) {
                        ++i;
                        continue;
                    }

                    const BlockData& blockData = BlockDataManager::getData(blockID);
                    glm::ivec2 texCoords = blockData.faces[direction].tex_coords;
                    float u0 = texCoords.x * TILE_WIDTH_NORMALIZED;
                    float v0 = texCoords.y * TILE_HEIGHT_NORMALIZED;

                    int quadW = 1;
                    for (int ww = 1; i + ww < size_u; ++ww) {
                        int tpos[3];
                        tpos[uAxis] = i + ww;
                        tpos[vAxis] = j;
                        tpos[normalAxis] = w;
                        int tx = tpos[0], ty = tpos[1], tz = tpos[2];
                        BlockID tblock = static_cast<BlockID>(data.getBlock(tx, ty, tz));
                        if (tblock != blockID || mask[i + ww][j]) break;
                        int tadj[3] = { tx + normalVec[0], ty + normalVec[1], tz + normalVec[2] };
                        unsigned char adjacentBlock = data.getBlock(tadj[0], tadj[1], tadj[2]);
                        if (isOpaque((unsigned char)tblock) == isOpaque(adjacentBlock)) break;
                        ++quadW;
                    }

                    int quadH = 1;
                    for (int hh = 1; j + hh < size_v; ++hh) {
                        bool canExtend = true;
                        for (int ww = 0; ww < quadW; ++ww) {
                            int tpos[3];
                            tpos[uAxis] = i + ww;
                            tpos[vAxis] = j + hh;
                            tpos[normalAxis] = w;
                            int tx = tpos[0], ty = tpos[1], tz = tpos[2];
                            BlockID tblock = static_cast<BlockID>(data.getBlock(tx, ty, tz));
                            if (tblock != blockID || mask[i + ww][j + hh]) {
                                canExtend = false;
                                break;
                            }
                            int tadj[3] = { tx + normalVec[0], ty + normalVec[1], tz + normalVec[2] };
                            unsigned char adjacentBlock = data.getBlock(tadj[0], tadj[1], tadj[2]);
                            if (isOpaque((unsigned char)tblock) == isOpaque(adjacentBlock)) {
                                canExtend = false;
                                break;
                            }
                        }
                        if (!canExtend) break;
                        ++quadH;
                    }

                    glm::ivec3 block_corner[4];
                    block_corner[0][uAxis] = i; block_corner[0][vAxis] = j; block_corner[0][normalAxis] = w;
                    block_corner[1][uAxis] = i + quadW; block_corner[1][vAxis] = j; block_corner[1][normalAxis] = w;
                    block_corner[2][uAxis] = i + quadW; block_corner[2][vAxis] = j + quadH; block_corner[2][normalAxis] = w;
                    block_corner[3][uAxis] = i; block_corner[3][vAxis] = j + quadH; block_corner[3][normalAxis] = w;

                    for (int vi = 0; vi < 4; ++vi) {
                        glm::ivec3 pos = block_corner[vi];
                        glm::ivec3 light_pos = pos;
                        light_pos[normalAxis] += sign;
                        lightVal[vi] = static_cast<float>(data.getLight(light_pos.x, light_pos.y, light_pos.z));

                        const int (*offsets)[3] = aoCheck[direction][vi];
                        bool s1 = isOpaque(data.getBlock(pos.x + offsets[0][0], pos.y + offsets[0][1], pos.z + offsets[0][2]));
                        bool s2 = isOpaque(data.getBlock(pos.x + offsets[1][0], pos.y + offsets[1][1], pos.z + offsets[1][2]));
                        bool c = isOpaque(data.getBlock(pos.x + offsets[2][0], pos.y + offsets[2][1], pos.z + offsets[2][2]));
                        ao[vi] = calculateAO(s1, s2, c);
                    }

                    float face_verts[4][3];
                    for (int vi = 0; vi < 4; ++vi) {
                        const float* src = faceVertices + (direction * 4 + vi) * 5;
                        face_verts[vi][0] = src[0];
                        face_verts[vi][1] = src[1];
                        face_verts[vi][2] = src[2];
                    }

                    float start_u = static_cast<float>(i);
                    float start_v = static_cast<float>(j);
                    float pos_n = static_cast<float>(w) + 0.5f + static_cast<float>(sign) * 0.5f;

                    if (ao[0] + ao[2] > ao[1] + ao[3]) {
                        indices.push_back(vertexCount);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 3);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 3);
                    }
                    else {
                        indices.push_back(vertexCount);
                        indices.push_back(vertexCount + 1);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 2);
                        indices.push_back(vertexCount + 3);
                        indices.push_back(vertexCount);
                    }
                    vertexCount += 4;

                    for (int hh = 0; hh < quadH; ++hh) {
                        for (int ww = 0; ww < quadW; ++ww) {
                            mask[i + ww][j + hh] = 1;
                        }
                    }

                    i += quadW;
                }
            }
        }
    }

    mesh.vertices = std::move(vertices);
    mesh.indices = std::move(indices);
}