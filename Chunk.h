#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "FaceData.h"

class World;

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 16;
const int CHUNK_DEPTH = 16;

class Chunk {
public:
    glm::ivec3 m_Position;
    bool m_IsMeshed = false; // NEW

    Chunk(int x, int y, int z);
    ~Chunk();

    void generateMesh(World& world);
    void uploadMesh();
    void draw();

    unsigned char getBlock(int x, int y, int z);
    void setBlock(int x, int y, int z, unsigned char blockID);

private:
    unsigned int VAO, VBO, EBO;
    std::vector<float> meshVertices;
    std::vector<unsigned int> meshIndices;
    unsigned char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH] = { 0 };
};