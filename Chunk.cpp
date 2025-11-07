#include "Chunk.h"
#include "World.h"
#include <iostream>

Chunk::Chunk(int x, int y, int z) : m_Position(x, y, z) {
    // The constructor is now very simple.
    // All block data is initialized to 0 (air) in the header.
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
}

Chunk::~Chunk() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

unsigned char Chunk::getBlock(int x, int y, int z) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return 0;
    }
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, unsigned char blockID) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return; // Safety check
    }
    blocks[x][y][z] = blockID;
}

void Chunk::generateMesh(World& world) {
    meshVertices.clear();
    meshIndices.clear();
    unsigned int vertexCount = 0;

    float worldPosX = (float)m_Position.x * CHUNK_WIDTH;
    float worldPosY = (float)m_Position.y * CHUNK_HEIGHT;
    float worldPosZ = (float)m_Position.z * CHUNK_DEPTH;

    for (int y = 0; y < CHUNK_HEIGHT; y++) {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int z = 0; z < CHUNK_DEPTH; z++) {
                if (blocks[x][y][z] == 0) continue;

                int blockWorldX = (int)worldPosX + x;
                int blockWorldY = (int)worldPosY + y;
                int blockWorldZ = (int)worldPosZ + z;

                auto addFace = [&](int faceIndex) {
                    for (int i = 0; i < 4; i++) {
                        int vIndex = (faceIndex * 4 + i) * 5;
                        meshVertices.push_back(faceVertices[vIndex + 0] + x + worldPosX);
                        meshVertices.push_back(faceVertices[vIndex + 1] + y + worldPosY);
                        meshVertices.push_back(faceVertices[vIndex + 2] + z + worldPosZ);
                        meshVertices.push_back(faceVertices[vIndex + 3]);
                        meshVertices.push_back(faceVertices[vIndex + 4]);
                    }
                    for (int i = 0; i < 6; i++) {
                        meshIndices.push_back(faceIndices[i] + vertexCount);
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

void Chunk::uploadMesh() {
    if (meshVertices.empty()) return;
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshIndices.size() * sizeof(unsigned int), meshIndices.data(), GL_DYNAMIC_DRAW);

    GLsizei stride = 5 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void Chunk::draw() {
    if (meshVertices.empty()) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshIndices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}