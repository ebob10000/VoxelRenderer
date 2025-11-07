#pragma once
#include "FastNoiseLite.h"
#include "Chunk.h"

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
    }

    void generateChunkData(Chunk& chunk) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {

                // Calculate absolute world coordinates for seamless noise
                float worldX = (float)(chunk.m_Position.x * CHUNK_WIDTH + x);
                float worldZ = (float)(chunk.m_Position.z * CHUNK_DEPTH + z);

                float noiseValue = m_Noise.GetNoise(worldX, worldZ);

                // Map noise from [-1, 1] to a world height [0, CHUNK_HEIGHT]
                int terrainHeight = static_cast<int>(((noiseValue + 1.0f) / 2.0f) * CHUNK_HEIGHT);

                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    int worldY = chunk.m_Position.y * CHUNK_HEIGHT + y;

                    if (worldY < terrainHeight) {
                        chunk.setBlock(x, y, z, 1); // 1 = Stone
                    }
                }
            }
        }
    }

private:
    FastNoiseLite m_Noise;
};