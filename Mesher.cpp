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

void SimpleMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh, bool smoothLighting) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    mesh.vertices.reserve(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * 2 * 4 * 7);
    mesh.indices.reserve(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * 2 * 6);

    float chunkWorldX = static_cast<float>(chunkPosition.x * CHUNK_WIDTH);
    float chunkWorldY = static_cast<float>(chunkPosition.y * CHUNK_HEIGHT);
    float chunkWorldZ = static_cast<float>(chunkPosition.z * CHUNK_DEPTH);

    auto addFace = [&](int x, int y, int z, int faceIndex) {
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
                bool s1 = data.getBlock(x + aoCheck[faceIndex][i][0][0], y + aoCheck[faceIndex][i][0][1], z + aoCheck[faceIndex][i][0][2]) != 0;
                bool s2 = data.getBlock(x + aoCheck[faceIndex][i][1][0], y + aoCheck[faceIndex][i][1][1], z + aoCheck[faceIndex][i][1][2]) != 0;
                bool c = data.getBlock(x + aoCheck[faceIndex][i][2][0], y + aoCheck[faceIndex][i][2][1], z + aoCheck[faceIndex][i][2][2]) != 0;
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
        }

        if (smoothLighting && (ao[0] + ao[2] > ao[1] + ao[3])) {
            mesh.indices.insert(mesh.indices.end(), { vertexCount, vertexCount + 1, vertexCount + 3, vertexCount + 1, vertexCount + 2, vertexCount + 3 });
        }
        else {
            mesh.indices.insert(mesh.indices.end(), { vertexCount, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount });
        }
        vertexCount += 4;
        };

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                BlockID currentBlock = (BlockID)data.getBlock(x, y, z);
                if (currentBlock == BlockID::Air) continue;

                if ((BlockID)data.getBlock(x + 1, y, z) == BlockID::Air) addFace(x, y, z, 1);
                if ((BlockID)data.getBlock(x - 1, y, z) == BlockID::Air) addFace(x, y, z, 0);
                if ((BlockID)data.getBlock(x, y + 1, z) == BlockID::Air) addFace(x, y, z, 3);
                if ((BlockID)data.getBlock(x, y - 1, z) == BlockID::Air) addFace(x, y, z, 2);
                if ((BlockID)data.getBlock(x, y, z + 1) == BlockID::Air) addFace(x, y, z, 5);
                if ((BlockID)data.getBlock(x, y, z - 1) == BlockID::Air) addFace(x, y, z, 4);
            }
        }
    }
}

namespace {
    struct FaceInfo {
        bool visible = false;
        unsigned char sunLight = 0;
        unsigned char blockLight = 0;
        BlockID blockID = BlockID::Air;

        bool operator==(const FaceInfo& other) const {
            return visible == other.visible && sunLight == other.sunLight && blockLight == other.blockLight && blockID == other.blockID;
        }
    };
}

void GreedyMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh, bool smoothLighting) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    mesh.vertices.reserve(4096 * 7);
    mesh.indices.reserve(2048 * 6);

    float chunkWorldX = static_cast<float>(chunkPosition.x * CHUNK_WIDTH);
    float chunkWorldY = static_cast<float>(chunkPosition.y * CHUNK_HEIGHT);
    float chunkWorldZ = static_cast<float>(chunkPosition.z * CHUNK_DEPTH);

    int dims[] = { CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH };

    for (int axis = 0; axis < 3; ++axis) {
        for (int dir = 0; dir < 2; ++dir) {
            bool positive = (dir == 1);
            int faceIndex = axis * 2 + dir;

            int u_axis = (axis + 1) % 3;
            int v_axis = (axis + 2) % 3;

            int D = dims[axis];
            int U = dims[u_axis];
            int V = dims[v_axis];

            for (int d = 0; d < D; ++d) {
                std::vector<FaceInfo> sliceData(U * V);

                for (int u = 0; u < U; ++u) {
                    for (int v = 0; v < V; ++v) {
                        int pos[3];
                        pos[axis] = d; pos[u_axis] = u; pos[v_axis] = v;

                        int normal[3] = { 0 }; normal[axis] = positive ? 1 : -1;
                        int nx = pos[0] + normal[0];
                        int ny = pos[1] + normal[1];
                        int nz = pos[2] + normal[2];

                        BlockID currentBlock = (BlockID)data.getBlock(pos[0], pos[1], pos[2]);
                        BlockID neighborBlock = (BlockID)data.getBlock(nx, ny, nz);

                        if (currentBlock != BlockID::Air && neighborBlock == BlockID::Air) {
                            FaceInfo& info = sliceData[u + v * U];
                            info.visible = true;
                            info.blockID = currentBlock;
                            info.sunLight = data.getSunlight(nx, ny, nz);
                            info.blockLight = data.getBlockLight(nx, ny, nz);
                        }
                    }
                }

                for (int v = 0; v < V; ++v) {
                    for (int u = 0; u < U; ++u) {
                        if (!sliceData[u + v * U].visible) continue;

                        FaceInfo currentFace = sliceData[u + v * U];
                        float lightLevel = std::max((float)currentFace.sunLight, (float)currentFace.blockLight);

                        int width = 1;
                        while (u + width < U && sliceData[u + width + v * U] == currentFace) {
                            width++;
                        }

                        int height = 1;
                        bool done = false;
                        for (int h = 1; v + h < V; ++h) {
                            for (int w = 0; w < width; ++w) {
                                if (!(sliceData[u + w + (v + h) * U] == currentFace)) { done = true; break; }
                            }
                            if (!done) height++; else break;
                        }

                        for (int h = 0; h < height; ++h) {
                            for (int w = 0; w < width; ++w) {
                                sliceData[u + w + (v + h) * U].visible = false;
                            }
                        }

                        const BlockData& blockData = BlockDataManager::getData(currentFace.blockID);
                        if (blockData.emissionStrength > 0) {
                            lightLevel = static_cast<float>(blockData.emissionStrength);
                        }
                        glm::ivec2 texCoords = blockData.faces[faceIndex].tex_coords;

                        float u_min = texCoords.x * TILE_WIDTH_NORMALIZED;
                        float v_min = texCoords.y * TILE_HEIGHT_NORMALIZED;

                        float ao[4] = { 0,0,0,0 };

                        float quad_pos[3]; quad_pos[axis] = static_cast<float>(d); quad_pos[u_axis] = static_cast<float>(u); quad_pos[v_axis] = static_cast<float>(v);
                        float du[3] = { 0 }, dv[3] = { 0 }; du[u_axis] = static_cast<float>(width); dv[v_axis] = static_cast<float>(height);

                        float offset = positive ? 0.5f : -0.5f;
                        float vert[4][3];
                        for (int i = 0; i < 3; ++i) {
                            float base = ((i == axis) ? (quad_pos[i] + offset) : (quad_pos[i] - 0.5f)) + 0.5f;
                            if (i == 0) base += chunkWorldX; else if (i == 1) base += chunkWorldY; else base += chunkWorldZ;
                            vert[0][i] = base; vert[1][i] = base + du[i]; vert[2][i] = base + du[i] + dv[i]; vert[3][i] = base + dv[i];
                        }

                        auto push_vert = [&](int vert_idx, float ao_val, float u_tex, float v_tex) {
                            mesh.vertices.insert(mesh.vertices.end(), { vert[vert_idx][0], vert[vert_idx][1], vert[vert_idx][2], u_tex, v_tex, ao_val, lightLevel });
                            };

                        float u_width = TILE_WIDTH_NORMALIZED * width;
                        float v_height = TILE_HEIGHT_NORMALIZED * height;

                        if (positive) {
                            push_vert(0, ao[0], u_min, v_min);
                            push_vert(1, ao[1], u_min + u_width, v_min);
                            push_vert(2, ao[2], u_min + u_width, v_min + v_height);
                            push_vert(3, ao[3], u_min, v_min + v_height);
                        }
                        else {
                            push_vert(0, ao[0], u_min, v_min);
                            push_vert(3, ao[3], u_min, v_min + v_height);
                            push_vert(2, ao[2], u_min + u_width, v_min + v_height);
                            push_vert(1, ao[1], u_min + u_width, v_min);
                        }

                        mesh.indices.insert(mesh.indices.end(), { vertexCount + 0, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount + 0 });
                        vertexCount += 4;
                    }
                }
            }
        }
    }
}