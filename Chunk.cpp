#include "Chunk.h"
#include "Mesh.h"
#include <cstring>

Chunk::Chunk(int x, int y, int z) : m_Position(x, y, z) {
    m_Mesh = std::make_unique<Mesh>();
}

void Chunk::draw() {
    if (m_Mesh) {
        m_Mesh->draw();
    }
}

unsigned char Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return 0;
    }
    return blocks[x][y][z];
}

void Chunk::setBlock(int x, int y, int z, unsigned char blockID) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return;
    }
    blocks[x][y][z] = blockID;
}

void Chunk::setBlocks(const unsigned char* data) {
    std::memcpy(blocks, data, sizeof(blocks));
}

unsigned char Chunk::getSunlight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return 15;
    }
    return lightLevels[x][y][z] >> 4;
}

void Chunk::setSunlight(int x, int y, int z, unsigned char lightLevel) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return;
    }
    lightLevels[x][y][z] = (lightLevels[x][y][z] & 0x0F) | (lightLevel << 4);
}

unsigned char Chunk::getBlockLight(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return 0;
    }
    return lightLevels[x][y][z] & 0x0F;
}

void Chunk::setBlockLight(int x, int y, int z, unsigned char lightLevel) {
    if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_HEIGHT || z < 0 || z >= CHUNK_DEPTH) {
        return;
    }
    lightLevels[x][y][z] = (lightLevels[x][y][z] & 0xF0) | lightLevel;
}

void Chunk::setLightLevels(const unsigned char* data) {
    std::memcpy(lightLevels, data, sizeof(lightLevels));
}