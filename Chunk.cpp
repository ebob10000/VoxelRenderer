#include "Chunk.h"
#include "Mesh.h"
#include "Mesher.h"
#include <cstring>

Chunk::Chunk(int x, int y, int z) : m_Position(x, y, z) {
    m_Mesh = std::make_unique<Mesh>();
}

void Chunk::createMesh(IMesher& mesher, World& world) {
    mesher.generateMesh(*this, world, *m_Mesh);
    m_Mesh->upload();
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