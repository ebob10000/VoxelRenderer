#include "Mesher.h"
#include "World.h"
#include "FaceData.h"
#include <iostream>

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
                    for (int i = 0; i < 4; i++) {
                        int vIndex = (faceIndex * 4 + i) * 5;
                        mesh.vertices.push_back(faceVertices[vIndex + 0] + blockWorldX);
                        mesh.vertices.push_back(faceVertices[vIndex + 1] + blockWorldY);
                        mesh.vertices.push_back(faceVertices[vIndex + 2] + blockWorldZ);
                        mesh.vertices.push_back(faceVertices[vIndex + 3]);
                        mesh.vertices.push_back(faceVertices[vIndex + 4]);
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

    std::cout << "SimpleMesher generated " << mesh.vertices.size() / 5 << " vertices, "
        << mesh.indices.size() / 3 << " triangles for chunk at ("
        << chunk.m_Position.x << ", " << chunk.m_Position.y << ", " << chunk.m_Position.z << ")" << std::endl;
}

void GreedyMesher::generateMesh(Chunk& chunk, World& world, Mesh& mesh) {
    mesh.vertices.clear();
    mesh.indices.clear();
    unsigned int vertexCount = 0;

    int chunkWorldX = chunk.m_Position.x * CHUNK_WIDTH;
    int chunkWorldY = chunk.m_Position.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunk.m_Position.z * CHUNK_DEPTH;

    // For each axis (X=0, Y=1, Z=2) and direction (negative=0, positive=1)
    for (int axis = 0; axis < 3; axis++) {
        for (int dir = 0; dir < 2; dir++) {
            bool positive = (dir == 1);

            // The two dimensions in the plane perpendicular to axis
            int u_axis = (axis + 1) % 3;
            int v_axis = (axis + 2) % 3;

            int size[3] = { CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH };

            // Iterate through each slice perpendicular to this axis
            for (int d = 0; d < size[axis]; d++) {
                bool mask[CHUNK_WIDTH * CHUNK_HEIGHT] = { false };

                // Build visibility mask for this slice
                for (int u = 0; u < size[u_axis]; u++) {
                    for (int v = 0; v < size[v_axis]; v++) {
                        // Convert back to x,y,z
                        int pos[3];
                        pos[axis] = d;
                        pos[u_axis] = u;
                        pos[v_axis] = v;

                        int x = pos[0], y = pos[1], z = pos[2];

                        unsigned char block = chunk.getBlock(x, y, z);
                        if (block == 0) continue;

                        // Check neighbor
                        int nx = x, ny = y, nz = z;
                        if (axis == 0) nx += positive ? 1 : -1;
                        else if (axis == 1) ny += positive ? 1 : -1;
                        else nz += positive ? 1 : -1;

                        // Convert to world coordinates for neighbor check
                        unsigned char neighbor = world.getBlock(
                            chunkWorldX + nx,
                            chunkWorldY + ny,
                            chunkWorldZ + nz
                        );

                        if (neighbor == 0) {
                            mask[u + v * size[u_axis]] = true;
                        }
                    }
                }

                // Greedy mesh this slice
                for (int v = 0; v < size[v_axis]; v++) {
                    for (int u = 0; u < size[u_axis]; u++) {
                        if (!mask[u + v * size[u_axis]]) continue;

                        // Compute width
                        int width = 1;
                        while (u + width < size[u_axis] && mask[u + width + v * size[u_axis]]) {
                            width++;
                        }

                        // Compute height
                        int height = 1;
                        bool done = false;
                        for (int h = 1; v + h < size[v_axis] && !done; h++) {
                            for (int w = 0; w < width; w++) {
                                if (!mask[u + w + (v + h) * size[u_axis]]) {
                                    done = true;
                                    break;
                                }
                            }
                            if (!done) height++;
                        }

                        // Clear the mask
                        for (int h = 0; h < height; h++) {
                            for (int w = 0; w < width; w++) {
                                mask[u + w + (v + h) * size[u_axis]] = false;
                            }
                        }

                        // Generate quad vertices
                        int x[3], du[3] = { 0 }, dv[3] = { 0 };
                        x[axis] = d;
                        x[u_axis] = u;
                        x[v_axis] = v;
                        du[u_axis] = width;
                        dv[v_axis] = height;

                        float offset = positive ? 0.5f : -0.5f;

                        float v0[3], v1[3], v2[3], v3[3];
                        for (int i = 0; i < 3; i++) {
                            float base = (i == axis) ? (x[i] + offset) : (x[i] - 0.5f);
                            if (i == 0) base += chunkWorldX;
                            else if (i == 1) base += chunkWorldY;
                            else base += chunkWorldZ;

                            v0[i] = base;
                            v1[i] = base + du[i];
                            v2[i] = base + du[i] + dv[i];
                            v3[i] = base + dv[i];
                        }

                        // Add quad with proper winding order
                        if (positive) {
                            mesh.vertices.insert(mesh.vertices.end(), { v0[0], v0[1], v0[2], 0.0f, 0.0f });
                            mesh.vertices.insert(mesh.vertices.end(), { v1[0], v1[1], v1[2], (float)width, 0.0f });
                            mesh.vertices.insert(mesh.vertices.end(), { v2[0], v2[1], v2[2], (float)width, (float)height });
                            mesh.vertices.insert(mesh.vertices.end(), { v3[0], v3[1], v3[2], 0.0f, (float)height });
                        }
                        else {
                            mesh.vertices.insert(mesh.vertices.end(), { v0[0], v0[1], v0[2], 0.0f, 0.0f });
                            mesh.vertices.insert(mesh.vertices.end(), { v3[0], v3[1], v3[2], 0.0f, (float)height });
                            mesh.vertices.insert(mesh.vertices.end(), { v2[0], v2[1], v2[2], (float)width, (float)height });
                            mesh.vertices.insert(mesh.vertices.end(), { v1[0], v1[1], v1[2], (float)width, 0.0f });
                        }

                        mesh.indices.insert(mesh.indices.end(), {
                            vertexCount + 0, vertexCount + 1, vertexCount + 2,
                            vertexCount + 2, vertexCount + 3, vertexCount + 0
                            });
                        vertexCount += 4;
                    }
                }
            }
        }
    }

    std::cout << "GreedyMesher generated " << mesh.vertices.size() / 5 << " vertices, "
        << mesh.indices.size() / 3 << " triangles for chunk at ("
        << chunk.m_Position.x << ", " << chunk.m_Position.y << ", " << chunk.m_Position.z << ")" << std::endl;
}