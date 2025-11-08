#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include <iostream>
#include <vector>

// --- ChunkMeshingData Implementation ---

ChunkMeshingData::ChunkMeshingData(World& world, const glm::ivec3& centralChunkPos) {
    std::shared_lock<std::shared_mutex> lock(world.m_ChunksMutex);
    int i = 0;
    for (int z = -1; z <= 1; ++z) {
        for (int x = -1; x <= 1; ++x) {
            glm::ivec3 pos = centralChunkPos + glm::ivec3(x, 0, z);
            auto it = world.m_Chunks.find(pos);
            if (it != world.m_Chunks.end()) {
                m_ChunkNeighbors[i] = it->second.get();
            }
            else {
                m_ChunkNeighbors[i] = nullptr;
            }
            i++;
        }
    }
}

unsigned char ChunkMeshingData::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0; // Air outside vertical bounds

    int chunkX = 1, chunkZ = 1;
    if (x < 0) chunkX = 0; else if (x >= CHUNK_WIDTH) chunkX = 2;
    if (z < 0) chunkZ = 0; else if (z >= CHUNK_DEPTH) chunkZ = 2;

    const Chunk* chunk = m_ChunkNeighbors[chunkX + chunkZ * 3];
    if (chunk) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return chunk->getBlock(localX, y, localZ);
    }

    return 1;
}

unsigned char ChunkMeshingData::getLight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;

    int chunkX = 1, chunkZ = 1;
    if (x < 0) chunkX = 0; else if (x >= CHUNK_WIDTH) chunkX = 2;
    if (z < 0) chunkZ = 0; else if (z >= CHUNK_DEPTH) chunkZ = 2;

    const Chunk* chunk = m_ChunkNeighbors[chunkX + chunkZ * 3];
    if (chunk) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return chunk->getLight(localX, y, localZ);
    }
    return 15;
}

// --- Mesher Shared Logic ---
namespace {
    float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) return 3.0f;
        return static_cast<float>(side1 + side2 + corner);
    }
}

// --- SimpleMesher Implementation ---

void SimpleMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    int chunkWorldX = chunkPosition.x * CHUNK_WIDTH;
    int chunkWorldY = chunkPosition.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunkPosition.z * CHUNK_DEPTH;

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                if (data.getBlock(x, y, z) == 0) continue;

                auto addFace = [&](int faceIndex) {
                    float lightLevel = static_cast<float>(data.getLight(x + faceNormals[faceIndex][0], y + faceNormals[faceIndex][1], z + faceNormals[faceIndex][2]));

                    for (int i = 0; i < 4; i++) {
                        int vIndex = (faceIndex * 4 + i);

                        mesh.vertices.insert(mesh.vertices.end(), {
                            faceVertices[vIndex * 5 + 0] + chunkWorldX + x,
                            faceVertices[vIndex * 5 + 1] + chunkWorldY + y,
                            faceVertices[vIndex * 5 + 2] + chunkWorldZ + z,
                            faceVertices[vIndex * 5 + 3],
                            faceVertices[vIndex * 5 + 4]
                            });

                        bool s1 = data.getBlock(x + aoCheck[faceIndex][i][0][0], y + aoCheck[faceIndex][i][0][1], z + aoCheck[faceIndex][i][0][2]) != 0;
                        bool s2 = data.getBlock(x + aoCheck[faceIndex][i][1][0], y + aoCheck[faceIndex][i][1][1], z + aoCheck[faceIndex][i][1][2]) != 0;
                        bool c = data.getBlock(x + aoCheck[faceIndex][i][2][0], y + aoCheck[faceIndex][i][2][1], z + aoCheck[faceIndex][i][2][2]) != 0;

                        mesh.vertices.push_back(calculateAO(s1, s2, c));
                        mesh.vertices.push_back(lightLevel);
                    }
                    mesh.indices.insert(mesh.indices.end(), { vertexCount, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount });
                    vertexCount += 4;
                    };

                if (data.getBlock(x - 1, y, z) == 0) addFace(0);
                if (data.getBlock(x + 1, y, z) == 0) addFace(1);
                if (data.getBlock(x, y - 1, z) == 0) addFace(2);
                if (data.getBlock(x, y + 1, z) == 0) addFace(3);
                if (data.getBlock(x, y, z - 1) == 0) addFace(4);
                if (data.getBlock(x, y, z + 1) == 0) addFace(5);
            }
        }
    }
}

// --- GreedyMesher Implementation ---
namespace {
    struct FaceInfo {
        bool visible = false;
        unsigned char light = 0;
        std::array<float, 4> ao{};

        bool operator==(const FaceInfo& other) const {
            return visible == other.visible && light == other.light && ao == other.ao;
        }
    };
}

void GreedyMesher::generateMesh(const ChunkMeshingData& data, const glm::ivec3& chunkPosition, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

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

                // Pass 1: Pre-calculate lighting and visibility for the entire slice
                for (int u = 0; u < CHUNK_WIDTH; ++u) {
                    for (int v = 0; v < CHUNK_DEPTH; ++v) {
                        int pos[3];
                        pos[axis] = d; pos[u_axis] = u; pos[v_axis] = v;

                        int normal[3] = { 0 }; normal[axis] = positive ? 1 : -1;

                        if (data.getBlock(pos[0], pos[1], pos[2]) != 0 && data.getBlock(pos[0] + normal[0], pos[1] + normal[1], pos[2] + normal[2]) == 0) {
                            FaceInfo& info = sliceData[u + v * CHUNK_WIDTH];
                            info.visible = true;
                            info.light = data.getLight(pos[0] + normal[0], pos[1] + normal[1], pos[2] + normal[2]);
                            for (int i = 0; i < 4; ++i) {
                                bool s1 = data.getBlock(pos[0] + aoCheck[faceIndex][i][0][0], pos[1] + aoCheck[faceIndex][i][0][1], pos[2] + aoCheck[faceIndex][i][0][2]) != 0;
                                bool s2 = data.getBlock(pos[0] + aoCheck[faceIndex][i][1][0], pos[1] + aoCheck[faceIndex][i][1][1], pos[2] + aoCheck[faceIndex][i][1][2]) != 0;
                                bool c = data.getBlock(pos[0] + aoCheck[faceIndex][i][2][0], pos[1] + aoCheck[faceIndex][i][2][1], pos[2] + aoCheck[faceIndex][i][2][2]) != 0;
                                info.ao[i] = calculateAO(s1, s2, c);
                            }
                        }
                    }
                }

                // Pass 2: Greedy meshing based on compatible lighting
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

                        float quad_pos[3]; quad_pos[axis] = static_cast<float>(d); quad_pos[u_axis] = static_cast<float>(u); quad_pos[v_axis] = static_cast<float>(v);
                        float du[3] = { 0 }, dv[3] = { 0 }; du[u_axis] = static_cast<float>(width); dv[v_axis] = static_cast<float>(height);

                        float offset = positive ? 0.5f : -0.5f;
                        float vert[4][3];
                        for (int i = 0; i < 3; ++i) {
                            float base = (i == axis) ? (quad_pos[i] + offset) : (quad_pos[i] - 0.5f);
                            if (i == 0) base += chunkWorldX; else if (i == 1) base += chunkWorldY; else base += chunkWorldZ;
                            vert[0][i] = base; vert[1][i] = base + du[i]; vert[2][i] = base + du[i] + dv[i]; vert[3][i] = base + dv[i];
                        }

                        auto push_vert = [&](int vert_idx, float ao_val) {
                            float u_tex, v_tex;
                            if (axis == 1) { u_tex = vert[vert_idx][0]; v_tex = vert[vert_idx][2]; }
                            else { u_tex = (axis == 0) ? vert[vert_idx][2] : vert[vert_idx][0]; v_tex = vert[vert_idx][1]; }
                            mesh.vertices.insert(mesh.vertices.end(), { vert[vert_idx][0], vert[vert_idx][1], vert[vert_idx][2], u_tex, v_tex, ao_val, static_cast<float>(currentFace.light) });
                            };

                        if (positive) {
                            push_vert(0, currentFace.ao[0]); push_vert(1, currentFace.ao[1]); push_vert(2, currentFace.ao[2]); push_vert(3, currentFace.ao[3]);
                        }
                        else {
                            push_vert(0, currentFace.ao[0]); push_vert(3, currentFace.ao[3]); push_vert(2, currentFace.ao[2]); push_vert(1, currentFace.ao[1]);
                        }

                        mesh.indices.insert(mesh.indices.end(), { vertexCount + 0, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount + 0 });
                        vertexCount += 4;
                    }
                }
            }
        }
    }
}