#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "FaceData.h"

// Forward declare World to avoid circular dependency
class World;

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 16;
const int CHUNK_DEPTH = 16;

class Chunk {
public:
    glm::ivec3 m_Position;

    Chunk(int x, int y, int z);
    ~Chunk();

    // The signature now takes a reference to the World
    void generateMesh(World& world);
    void uploadMesh();
    void draw();

    // Helper for the World to get a block from this chunk
    unsigned char getBlock(int x, int y, int z);

private:
    unsigned int VAO, VBO, EBO;
    std::vector<float> meshVertices;
    std::vector<unsigned int> meshIndices;
    unsigned char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
};