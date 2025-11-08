#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include "Block.h" // Include our new block definitions
#include <iostream>
#include <vector>
#include <cstring>

// --- Constants for Texture Atlas ---
// IMPORTANT: Update these if you change your atlas.png dimensions
const float ATLAS_WIDTH_TILES = 2.0f;
const float ATLAS_HEIGHT_TILES = 2.0f;
const float TILE_WIDTH_NORMALIZED = 1.0f / ATLAS_WIDTH_TILES;
const float TILE_HEIGHT_NORMALIZED = 1.0f / ATLAS_HEIGHT_TILES;

// --- ChunkMeshingData Implementation ---

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

    if (neighbors[4]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                for (int x = 0; x < CHUNK_WIDTH; ++x) {
                    m_Blocks[x + 1][y][z + 1] = neighbors[4]->getBlock(x, y, z);
                    m_LightLevels[x + 1][y][z + 1] = neighbors[4]->getLight(x, y, z);
                }
            }
        }
    }

    if (neighbors[3]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[0][y][z + 1] = neighbors[3]->getBlock(CHUNK_WIDTH - 1, y, z);
            m_LightLevels[0][y][z + 1] = neighbors[3]->getLight(CHUNK_WIDTH - 1, y, z);
        }
    }
    if (neighbors[5]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int z = 0; z < CHUNK_DEPTH; ++z) {
            m_Blocks[CHUNK_WIDTH + 1][y][z + 1] = neighbors[5]->getBlock(0, y, z);
            m_LightLevels[CHUNK_WIDTH + 1][y][z + 1] = neighbors[5]->getLight(0, y, z);
        }
    }
    if (neighbors[1]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][0] = neighbors[1]->getBlock(x, y, CHUNK_DEPTH - 1);
            m_LightLevels[x + 1][y][0] = neighbors[1]->getLight(x, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[7]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
            m_Blocks[x + 1][y][CHUNK_DEPTH + 1] = neighbors[7]->getBlock(x, y, 0);
            m_LightLevels[x + 1][y][CHUNK_DEPTH + 1] = neighbors[7]->getLight(x, y, 0);
        }
    }

    if (neighbors[0]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[0][y][0] = neighbors[0]->getBlock(CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1);
            m_LightLevels[0][y][0] = neighbors[0]->getLight(CHUNK_WIDTH - 1, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[2]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[CHUNK_WIDTH + 1][y][0] = neighbors[2]->getBlock(0, y, CHUNK_DEPTH - 1);
            m_LightLevels[CHUNK_WIDTH + 1][y][0] = neighbors[2]->getLight(0, y, CHUNK_DEPTH - 1);
        }
    }
    if (neighbors[6]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[0][y][CHUNK_DEPTH + 1] = neighbors[6]->getBlock(CHUNK_WIDTH - 1, y, 0);
            m_LightLevels[0][y][CHUNK_DEPTH + 1] = neighbors[6]->getLight(CHUNK_WIDTH - 1, y, 0);
        }
    }
    if (neighbors[8]) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            m_Blocks[CHUNK_WIDTH + 1][y][CHUNK_DEPTH + 1] = neighbors[8]->getBlock(0, y, 0);
            m_LightLevels[CHUNK_WIDTH + 1][y][CHUNK_DEPTH + 1] = neighbors[8]->getLight(0, y, 0);
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


// --- Mesher Shared Logic ---
namespace {
    float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) {
            return 3.0f;
        }
        return static_cast<float>(side1 + side2 + corner);
    }
}

// --- SimpleMesher Implementation ---
void SimpleMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    mesh.vertices.reserve(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * 2 * 4 * 7);
    mesh.indices.reserve(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH * 2 * 6);

    int chunkWorldX = chunkPosition.x * CHUNK_WIDTH;
    int chunkWorldY = chunkPosition.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunkPosition.z * CHUNK_DEPTH;

    auto addFace = [&](int x, int y, int z, int faceIndex) {
        BlockID blockID = (BlockID)data.getBlock(x, y, z);
        const BlockData& blockData = BlockDataManager::getData(blockID);
        glm::ivec2 texCoords = blockData.faces[faceIndex].tex_coords;

        float u_min = texCoords.x * TILE_WIDTH_NORMALIZED;
        float v_min = texCoords.y * TILE_HEIGHT_NORMALIZED;
        float u_max = u_min + TILE_WIDTH_NORMALIZED;
        float v_max = v_min + TILE_HEIGHT_NORMALIZED;

        const float faceUVs[4][2] = {
            {u_min, v_min}, {u_max, v_min}, {u_max, v_max}, {u_min, v_max}
        };

        float lightLevel = static_cast<float>(data.getLight(x + faceNormals[faceIndex][0], y + faceNormals[faceIndex][1], z + faceNormals[faceIndex][2]));
        float ao[4];

        for (int i = 0; i < 4; i++) {
            int vIndex = (faceIndex * 4 + i);

            mesh.vertices.insert(mesh.vertices.end(), {
                faceVertices[vIndex * 5 + 0] + chunkWorldX + x,
                faceVertices[vIndex * 5 + 1] + chunkWorldY + y,
                faceVertices[vIndex * 5 + 2] + chunkWorldZ + z,
                faceUVs[i][0],
                faceUVs[i][1]
                });

            bool s1 = data.getBlock(x + aoCheck[faceIndex][i][0][0], y + aoCheck[faceIndex][i][0][1], z + aoCheck[faceIndex][i][0][2]) != 0;
            bool s2 = data.getBlock(x + aoCheck[faceIndex][i][1][0], y + aoCheck[faceIndex][i][1][1], z + aoCheck[faceIndex][i][1][2]) != 0;
            bool c = data.getBlock(x + aoCheck[faceIndex][i][2][0], y + aoCheck[faceIndex][i][2][1], z + aoCheck[faceIndex][i][2][2]) != 0;

            ao[i] = calculateAO(s1, s2, c);
            mesh.vertices.push_back(ao[i]);
            mesh.vertices.push_back(lightLevel);
        }

        if (ao[0] + ao[2] > ao[1] + ao[3]) {
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
                if (currentBlock != BlockID::Air) continue;

                auto checkNeighbor = [&](int nx, int ny, int nz, int face) {
                    BlockID neighborID = (BlockID)data.getBlock(nx, ny, nz);
                    if (neighborID != BlockID::Air) {
                        addFace(nx, ny, nz, face);
                    }
                    };

                checkNeighbor(x - 1, y, z, 1); // +X face on neighbor
                checkNeighbor(x + 1, y, z, 0); // -X face on neighbor
                checkNeighbor(x, y - 1, z, 3); // +Y face on neighbor
                checkNeighbor(x, y + 1, z, 2); // -Y face on neighbor
                checkNeighbor(x, y, z - 1, 5); // +Z face on neighbor
                checkNeighbor(x, y, z + 1, 4); // -Z face on neighbor
            }
        }
    }
}

// --- GreedyMesher Implementation ---
namespace {
    struct FaceInfo {
        bool visible = false;
        unsigned char light = 0;
        BlockID blockID = BlockID::Air;

        bool operator==(const FaceInfo& other) const {
            return visible == other.visible && light == other.light && blockID == other.blockID;
        }
    };
}

void GreedyMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    mesh.vertices.reserve(4096 * 7);
    mesh.indices.reserve(2048 * 6);

    int chunkWorldX = chunkPosition.x * CHUNK_WIDTH;
    int chunkWorldY = chunkPosition.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunkPosition.z * CHUNK_DEPTH;

    for (int axis = 0; axis < 3; ++axis) {
        for (int dir = 0; dir < 2; ++dir) {
            bool positive = (dir == 1);
            int faceIndex = axis * 2 + dir;

            int u_axis = (axis + 1) % 3;
            int v_axis = (axis + 2) % 3;

            for (int d = 0; d < CHUNK_HEIGHT; ++d) {
                std::vector<FaceInfo> sliceData(CHUNK_WIDTH * CHUNK_DEPTH);

                for (int u = 0; u < CHUNK_WIDTH; ++u) {
                    for (int v = 0; v < CHUNK_DEPTH; ++v) {
                        int pos[3];
                        pos[axis] = d; pos[u_axis] = u; pos[v_axis] = v;

                        int normal[3] = { 0 }; normal[axis] = positive ? 1 : -1;

                        BlockID currentBlock = (BlockID)data.getBlock(pos[0], pos[1], pos[2]);
                        BlockID neighborBlock = (BlockID)data.getBlock(pos[0] + normal[0], pos[1] + normal[1], pos[2] + normal[2]);

                        if (currentBlock != BlockID::Air && neighborBlock == BlockID::Air) {
                            FaceInfo& info = sliceData[u + v * CHUNK_WIDTH];
                            info.visible = true;
                            info.blockID = currentBlock;
                            info.light = data.getLight(pos[0] + normal[0], pos[1] + normal[1], pos[2] + normal[2]);
                        }
                    }
                }

                for (int v = 0; v < CHUNK_DEPTH; ++v) {
                    for (int u = 0; u < CHUNK_WIDTH; ++u) {
                        if (!sliceData[u + v * CHUNK_WIDTH].visible) continue;

                        FaceInfo currentFace = sliceData[u + v * CHUNK_WIDTH];

                        int width = 1;
                        while (u + width < CHUNK_WIDTH && sliceData[u + width + v * CHUNK_WIDTH] == currentFace) {
                            width++;
                        }

                        int height = 1;
                        bool done = false;
                        for (int h = 1; v + h < CHUNK_DEPTH; ++h) {
                            for (int w = 0; w < width; ++w) {
                                if (!(sliceData[u + w + (v + h) * CHUNK_WIDTH] == currentFace)) { done = true; break; }
                            }
                            if (!done) height++; else break;
                        }

                        for (int h = 0; h < height; ++h) {
                            for (int w = 0; w < width; ++w) {
                                sliceData[u + w + (v + h) * CHUNK_WIDTH].visible = false;
                            }
                        }

                        const BlockData& blockData = BlockDataManager::getData(currentFace.blockID);
                        glm::ivec2 texCoords = blockData.faces[faceIndex].tex_coords;

                        float u_min = texCoords.x * TILE_WIDTH_NORMALIZED;
                        float v_min = texCoords.y * TILE_HEIGHT_NORMALIZED;

                        float ao[4];
                        int corner_blocks[4][3];
                        corner_blocks[0][axis] = d; corner_blocks[0][u_axis] = u; corner_blocks[0][v_axis] = v;
                        corner_blocks[1][axis] = d; corner_blocks[1][u_axis] = u + width - 1; corner_blocks[1][v_axis] = v;
                        corner_blocks[2][axis] = d; corner_blocks[2][u_axis] = u + width - 1; corner_blocks[2][v_axis] = v + height - 1;
                        corner_blocks[3][axis] = d; corner_blocks[3][u_axis] = u; corner_blocks[3][v_axis] = v + height - 1;

                        int canonical_indices[] = { 0, 1, 2, 3 };

                        for (int i = 0; i < 4; ++i) {
                            bool s1 = data.getBlock(corner_blocks[i][0] + aoCheck[faceIndex][canonical_indices[i]][0][0], corner_blocks[i][1] + aoCheck[faceIndex][canonical_indices[i]][0][1], corner_blocks[i][2] + aoCheck[faceIndex][canonical_indices[i]][0][2]) != 0;
                            bool s2 = data.getBlock(corner_blocks[i][0] + aoCheck[faceIndex][canonical_indices[i]][1][0], corner_blocks[i][1] + aoCheck[faceIndex][canonical_indices[i]][1][1], corner_blocks[i][2] + aoCheck[faceIndex][canonical_indices[i]][1][2]) != 0;
                            bool c = data.getBlock(corner_blocks[i][0] + aoCheck[faceIndex][canonical_indices[i]][2][0], corner_blocks[i][1] + aoCheck[faceIndex][canonical_indices[i]][2][1], corner_blocks[i][2] + aoCheck[faceIndex][canonical_indices[i]][2][2]) != 0;
                            ao[i] = calculateAO(s1, s2, c);
                        }

                        float quad_pos[3]; quad_pos[axis] = static_cast<float>(d); quad_pos[u_axis] = static_cast<float>(u); quad_pos[v_axis] = static_cast<float>(v);
                        float du[3] = { 0 }, dv[3] = { 0 }; du[u_axis] = static_cast<float>(width); dv[v_axis] = static_cast<float>(height);

                        float offset = positive ? 0.5f : -0.5f;
                        float vert[4][3];
                        for (int i = 0; i < 3; ++i) {
                            float base = (i == axis) ? (quad_pos[i] + offset) : (quad_pos[i] - 0.5f);
                            if (i == 0) base += chunkWorldX; else if (i == 1) base += chunkWorldY; else base += chunkWorldZ;
                            vert[0][i] = base; vert[1][i] = base + du[i]; vert[2][i] = base + du[i] + dv[i]; vert[3][i] = base + dv[i];
                        }

                        auto push_vert = [&](int vert_idx, float ao_val, float u_tex, float v_tex) {
                            mesh.vertices.insert(mesh.vertices.end(), { vert[vert_idx][0], vert[vert_idx][1], vert[vert_idx][2], u_tex, v_tex, ao_val, static_cast<float>(currentFace.light) });
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

                        if (ao[0] + ao[2] > ao[1] + ao[3]) {
                            mesh.indices.insert(mesh.indices.end(), { vertexCount + 0, vertexCount + 1, vertexCount + 3, vertexCount + 1, vertexCount + 2, vertexCount + 3 });
                        }
                        else {
                            mesh.indices.insert(mesh.indices.end(), { vertexCount + 0, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount + 0 });
                        }
                        vertexCount += 4;
                    }
                }
            }
        }
    }
}