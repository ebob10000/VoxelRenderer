#pragma once
#include <vector>
#include <iostream>
#include <glad/glad.h>
#include "FaceData.h"

const int CHUNK_WIDTH = 16, CHUNK_HEIGHT = 16, CHUNK_DEPTH = 16;

class Chunk {
public:
    // ... (constructor, destructor, and other members are the same) ...
    unsigned int VAO, VBO, EBO;
    std::vector<float> meshVertices;
    std::vector<unsigned int> meshIndices;
    unsigned char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
    bool needsUpdate = true;

    Chunk() {
        for (int x = 0; x < CHUNK_WIDTH; x++)
            for (int y = 0; y < CHUNK_HEIGHT; y++)
                for (int z = 0; z < CHUNK_DEPTH; z++)
                    blocks[x][y][z] = (y < 8) ? 1 : 0;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    ~Chunk() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    // UPDATED generateMesh
    void generateMesh() {
        meshVertices.clear();
        meshIndices.clear();
        unsigned int vertexCount = 0;

        for (int y = 0; y < CHUNK_HEIGHT; y++) {
            for (int x = 0; x < CHUNK_WIDTH; x++) {
                for (int z = 0; z < CHUNK_DEPTH; z++) {
                    if (blocks[x][y][z] == 0) continue;

                    // Check neighbors in all 6 directions
                    int dx[] = { -1, 1, 0, 0, 0, 0 };
                    int dy[] = { 0, 0, -1, 1, 0, 0 };
                    int dz[] = { 0, 0, 0, 0, -1, 1 };

                    for (int face = 0; face < 6; ++face) {
                        int checkX = x + dx[face];
                        int checkY = y + dy[face];
                        int checkZ = z + dz[face];
                        bool isVisible = false;

                        if (checkX < 0 || checkX >= CHUNK_WIDTH || checkY < 0 || checkY >= CHUNK_HEIGHT || checkZ < 0 || checkZ >= CHUNK_DEPTH) {
                            isVisible = true;
                        }
                        else if (blocks[checkX][checkY][checkZ] == 0) {
                            isVisible = true;
                        }

                        if (isVisible) {
                            // A vertex is now 5 floats (X,Y,Z,U,V)
                            for (int i = 0; i < 4; i++) {
                                int vertexIndex = (face * 4 + i) * 5; // Stride is 5
                                // Position
                                meshVertices.push_back(faceVertices[vertexIndex + 0] + x);
                                meshVertices.push_back(faceVertices[vertexIndex + 1] + y);
                                meshVertices.push_back(faceVertices[vertexIndex + 2] + z);
                                // Texture Coordinate
                                meshVertices.push_back(faceVertices[vertexIndex + 3]);
                                meshVertices.push_back(faceVertices[vertexIndex + 4]);
                            }
                            for (int i = 0; i < 6; i++) {
                                meshIndices.push_back(faceIndices[i] + vertexCount);
                            }
                            vertexCount += 4;
                        }
                    }
                }
            }
        }
        needsUpdate = false;
        std::cout << "Generated textured chunk mesh with " << meshVertices.size() / 5 << " vertices.\n";
    }

    // UPDATED uploadMesh
    void uploadMesh() {
        if (meshVertices.empty()) return;

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshIndices.size() * sizeof(unsigned int), meshIndices.data(), GL_DYNAMIC_DRAW);

        // --- This is the critical change ---
        // Stride is now 5 floats (X,Y,Z,U,V)
        GLsizei stride = 5 * sizeof(float);

        // Position attribute (location 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);

        // Texture Coordinate attribute (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void draw() {
        if (meshIndices.empty()) return;
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshIndices.size()), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};