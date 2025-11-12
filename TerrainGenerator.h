#pragma once
#include "FastNoiseLite.h"
#include "Chunk.h"
#include "Block.h"
#include <cstdlib>
#include <ctime>

class TerrainGenerator {
public:
    TerrainGenerator(int seed) {
        m_Noise.SetSeed(seed);
        m_Noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_Noise.SetFrequency(0.005f);
        m_Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        m_Noise.SetFractalOctaves(5);
        m_Noise.SetFractalLacunarity(2.0f);
        m_Noise.SetFractalGain(0.5f);

        srand(seed);
    }

    void generateChunkData(Chunk& chunk) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {

                float worldX = (float)(chunk.m_Position.x * CHUNK_WIDTH + x);
                float worldZ = (float)(chunk.m_Position.z * CHUNK_DEPTH + z);
                float noiseValue = m_Noise.GetNoise(worldX, worldZ);
                int terrainHeight = static_cast<int>(((noiseValue + 1.0f) / 2.0f) * (CHUNK_HEIGHT - 10) + 5);

                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    int worldY = chunk.m_Position.y * CHUNK_HEIGHT + y;

                    if (worldY > terrainHeight) {
                        chunk.setBlock(x, y, z, (unsigned char)BlockID::Air);
                    }
                    else if (worldY == terrainHeight) {
                        chunk.setBlock(x, y, z, (unsigned char)BlockID::Grass);
                    }
                    else if (worldY > terrainHeight - 3) {
                        chunk.setBlock(x, y, z, (unsigned char)BlockID::Dirt);
                    }
                    else {
                        chunk.setBlock(x, y, z, (unsigned char)BlockID::Stone);
                    }
                }
            }
        }

        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                for (int y = 0; y < 6; ++y) {
                    int chance = -1;
                    if (y == 0)      chance = 100;
                    else if (y == 1) chance = 80;
                    else if (y == 2) chance = 60;
                    else if (y == 3) chance = 40;
                    else if (y == 5) chance = 20;

                    if (chance > 0) {
                        if ((rand() % 100) < chance) {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Bedrock);
                        }
                    }
                }
            }
        }
    }

private:
    FastNoiseLite m_Noise;
};