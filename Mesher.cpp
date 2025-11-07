#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include <iostream>
#include <array>

namespace {
    float calculateAO(bool side1, bool side2, bool corner) {
        if (side1 && side2) {
            return 3.0f;
        }
        return static_cast<float>(side1 + side2 + corner);
    }
}

void SimpleMesher::generateMesh(Chunk& chunk, World& world, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                if (chunk.getBlock(x, y, z) == 0) continue;

                int blockWorldX = chunk.m_Position.x * CHUNK_WIDTH + x;
                int blockWorldY = chunk.m_Position.y * CHUNK_HEIGHT + y;
                int blockWorldZ = chunk.m_Position.z * CHUNK_DEPTH + z;

                auto addFace = [&](int faceIndex) {
                    float lightLevel = world.getLight(blockWorldX + faceNormals[faceIndex][0], blockWorldY + faceNormals[faceIndex][1], blockWorldZ + faceNormals[faceIndex][2]);

                    for (int i = 0; i < 4; i++) {
                        int vIndex = (faceIndex * 4 + i);

                        float v_x = faceVertices[vIndex * 5 + 0] + blockWorldX;
                        float v_y = faceVertices[vIndex * 5 + 1] + blockWorldY;
                        float v_z = faceVertices[vIndex * 5 + 2] + blockWorldZ;

                        mesh.vertices.push_back(v_x);
                        mesh.vertices.push_back(v_y);
                        mesh.vertices.push_back(v_z);
                        mesh.vertices.push_back(faceVertices[vIndex * 5 + 3]);
                        mesh.vertices.push_back(faceVertices[vIndex * 5 + 4]);

                        bool s1 = world.getBlock(blockWorldX + aoCheck[faceIndex][i][0][0], blockWorldY + aoCheck[faceIndex][i][0][1], blockWorldZ + aoCheck[faceIndex][i][0][2]) != 0;
                        bool s2 = world.getBlock(blockWorldX + aoCheck[faceIndex][i][1][0], blockWorldY + aoCheck[faceIndex][i][1][1], blockWorldZ + aoCheck[faceIndex][i][1][2]) != 0;
                        bool c = world.getBlock(blockWorldX + aoCheck[faceIndex][i][2][0], blockWorldY + aoCheck[faceIndex][i][2][1], blockWorldZ + aoCheck[faceIndex][i][2][2]) != 0;

                        mesh.vertices.push_back(calculateAO(s1, s2, c));
                        mesh.vertices.push_back(lightLevel);
                    }
                    for (int i = 0; i < 6; i++) {
                        mesh.indices.push_back(faceIndices[i] + vertexCount);
                    }
                    vertexCount += 4;
                    };

                if (world.getBlock(blockWorldX - 1, blockWorldY, blockWorldZ) == 0) addFace(0);
                if (world.getBlock(blockWorldX + 1, blockWorldY, blockWorldZ) == 0) addFace(1);
                if (world.getBlock(blockWorldX, blockWorldY - 1, blockWorldZ) == 0) addFace(2);
                if (world.getBlock(blockWorldX, blockWorldY + 1, blockWorldZ) == 0) addFace(3);
                if (world.getBlock(blockWorldX, blockWorldY, blockWorldZ - 1) == 0) addFace(4);
                if (world.getBlock(blockWorldX, blockWorldY, blockWorldZ + 1) == 0) addFace(5);
            }
        }
    }
}

void GreedyMesher::generateMesh(Chunk& chunk, World& world, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    int chunkWorldX = chunk.m_Position.x * CHUNK_WIDTH;
    int chunkWorldY = chunk.m_Position.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunk.m_Position.z * CHUNK_DEPTH;

    for (int axis = 0; axis < 3; axis++) {
        for (int dir = 0; dir < 2; dir++) {
            bool positive = (dir == 1);

            int u_axis = (axis + 1) % 3;
            int v_axis = (axis + 2) % 3;

            int size[3] = { CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH };

            for (int d = 0; d < size[axis]; d++) {
                bool mask[CHUNK_WIDTH * CHUNK_HEIGHT] = { false };

                for (int u = 0; u < size[u_axis]; u++) {
                    for (int v = 0; v < size[v_axis]; v++) {
                        int pos[3];
                        pos[axis] = d;
                        pos[u_axis] = u;
                        pos[v_axis] = v;

                        int x = pos[0], y = pos[1], z = pos[2];
                        if (chunk.getBlock(x, y, z) == 0) continue;

                        int nx = x, ny = y, nz = z;
                        if (axis == 0) { nx += positive ? 1 : -1; }
                        else if (axis == 1) { ny += positive ? 1 : -1; }
                        else { nz += positive ? 1 : -1; }

                        if (world.getBlock(chunkWorldX + nx, chunkWorldY + ny, chunkWorldZ + nz) == 0) {
                            mask[u + v * size[u_axis]] = true;
                        }
                    }
                }

                for (int v_iter = 0; v_iter < size[v_axis]; v_iter++) {
                    for (int u_iter = 0; u_iter < size[u_axis]; u_iter++) {
                        if (!mask[u_iter + v_iter * size[u_axis]]) continue;

                        int width = 1;
                        while (u_iter + width < size[u_axis] && mask[u_iter + width + v_iter * size[u_axis]]) {
                            width++;
                        }

                        int height = 1;
                        bool done = false;
                        for (int h = 1; v_iter + h < size[v_axis] && !done; h++) {
                            for (int w = 0; w < width; w++) {
                                if (!mask[u_iter + w + (v_iter + h) * size[u_axis]]) {
                                    done = true;
                                    break;
                                }
                            }
                            if (!done) height++;
                        }

                        for (int h = 0; h < height; h++) {
                            for (int w = 0; w < width; w++) {
                                mask[u_iter + w + (v_iter + h) * size[u_axis]] = false;
                            }
                        }

                        float quad_pos[3];
                        quad_pos[axis] = static_cast<float>(d);
                        quad_pos[u_axis] = static_cast<float>(u_iter);
                        quad_pos[v_axis] = static_cast<float>(v_iter);

                        float du[3] = { 0 }, dv[3] = { 0 };
                        du[u_axis] = static_cast<float>(width);
                        dv[v_axis] = static_cast<float>(height);

                        int normal[3] = { 0 };
                        normal[axis] = positive ? 1 : -1;

                        float lightLevel = world.getLight(
                            chunkWorldX + static_cast<int>(quad_pos[0]) + normal[0],
                            chunkWorldY + static_cast<int>(quad_pos[1]) + normal[1],
                            chunkWorldZ + static_cast<int>(quad_pos[2]) + normal[2])
                            ;

                        float ao[4];
                        int faceIndex = axis * 2 + dir;

                        std::array<int, 3> base_pos = { static_cast<int>(quad_pos[0]), static_cast<int>(quad_pos[1]), static_cast<int>(quad_pos[2]) };
                        std::array<int, 3> du_a = { static_cast<int>(du[0]), static_cast<int>(du[1]), static_cast<int>(du[2]) };
                        std::array<int, 3> dv_a = { static_cast<int>(dv[0]), static_cast<int>(dv[1]), static_cast<int>(dv[2]) };

                        std::array<int, 3> corner_pos[4] = {
                            base_pos,
                            {base_pos[0] + du_a[0], base_pos[1] + du_a[1], base_pos[2] + du_a[2]},
                            {base_pos[0] + du_a[0] + dv_a[0], base_pos[1] + du_a[1] + dv_a[1], base_pos[2] + du_a[2] + dv_a[2]},
                            {base_pos[0] + dv_a[0], base_pos[1] + dv_a[1], base_pos[2] + dv_a[2]}
                        };

                        for (int i = 0; i < 4; ++i) {
                            bool s1 = world.getBlock(chunkWorldX + corner_pos[i][0] + aoCheck[faceIndex][i][0][0], chunkWorldY + corner_pos[i][1] + aoCheck[faceIndex][i][0][1], chunkWorldZ + corner_pos[i][2] + aoCheck[faceIndex][i][0][2]) != 0;
                            bool s2 = world.getBlock(chunkWorldX + corner_pos[i][0] + aoCheck[faceIndex][i][1][0], chunkWorldY + corner_pos[i][1] + aoCheck[faceIndex][i][1][1], chunkWorldZ + corner_pos[i][2] + aoCheck[faceIndex][i][1][2]) != 0;
                            bool c = world.getBlock(chunkWorldX + corner_pos[i][0] + aoCheck[faceIndex][i][2][0], chunkWorldY + corner_pos[i][1] + aoCheck[faceIndex][i][2][1], chunkWorldZ + corner_pos[i][2] + aoCheck[faceIndex][i][2][2]) != 0;
                            ao[i] = calculateAO(s1, s2, c);
                        }

                        float offset = positive ? 0.5f : -0.5f;
                        float v[4][3];
                        for (int i = 0; i < 3; i++) {
                            float base = (i == axis) ? (quad_pos[i] + offset) : (quad_pos[i] - 0.5f);
                            if (i == 0) base += chunkWorldX; else if (i == 1) base += chunkWorldY; else base += chunkWorldZ;
                            v[0][i] = base;
                            v[1][i] = base + du[i];
                            v[2][i] = base + du[i] + dv[i];
                            v[3][i] = base + dv[i];
                        }

                        auto push_vert = [&](int i, float u_tex, float v_tex, float ao_val) {
                            mesh.vertices.insert(mesh.vertices.end(), { v[i][0], v[i][1], v[i][2], u_tex, v_tex, ao_val, lightLevel });
                            };

                        if (positive) {
                            push_vert(0, 0, 0, ao[0]);
                            push_vert(1, static_cast<float>(width), 0, ao[1]);
                            push_vert(2, static_cast<float>(width), static_cast<float>(height), ao[2]);
                            push_vert(3, 0, static_cast<float>(height), ao[3]);
                        }
                        else {
                            push_vert(0, 0, 0, ao[0]);
                            push_vert(3, 0, static_cast<float>(height), ao[3]);
                            push_vert(2, static_cast<float>(width), static_cast<float>(height), ao[2]);
                            push_vert(1, static_cast<float>(width), 0, ao[1]);
                        }

                        mesh.indices.insert(mesh.indices.end(), { vertexCount + 0, vertexCount + 1, vertexCount + 2, vertexCount + 2, vertexCount + 3, vertexCount + 0 });
                        vertexCount += 4;
                    }
                }
            }
        }
    }
}