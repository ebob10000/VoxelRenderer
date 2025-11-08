#pragma once
#include <vector>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Mesh;

const int CHUNK_WIDTH = 16;
const int CHUNK_HEIGHT = 64;
const int CHUNK_DEPTH = 16;

class Chunk {
public:
    const glm::ivec3 m_Position;
    std::unique_ptr<Mesh> m_Mesh;
    unsigned char blocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH] = { 0 };

    Chunk(int x, int y, int z);

    void draw();

    unsigned char getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, unsigned char blockID);

    const unsigned char* getBlocks() const { return &blocks[0][0][0]; }
    void setBlocks(const unsigned char* data);

    unsigned char getLight(int x, int y, int z) const;
    void setLight(int x, int y, int z, unsigned char lightLevel);

private:
    unsigned char lightLevels[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH] = { 0 };
};