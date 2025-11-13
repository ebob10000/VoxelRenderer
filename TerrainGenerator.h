#pragma once
#include "FastNoiseLite.h"
#include "Chunk.h"
#include "Block.h"
#include <cstdlib>
#include <ctime>
#include <algorithm> // For std::max/min
#include <cmath>     // For pow

class TerrainGenerator {
public:
    enum class Biome {
        Ocean,
        Plains,
        Forest
    };

    TerrainGenerator(int seed) : m_Seed(seed) {
        srand(seed);

        // --- Core Terrain Noise ---
        m_ContinentalnessNoise.SetSeed(seed);
        m_ContinentalnessNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_ContinentalnessNoise.SetFrequency(0.0008f);

        m_TerrainNoise.SetSeed(seed + 1);
        m_TerrainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_TerrainNoise.SetFrequency(0.004f);
        m_TerrainNoise.SetFractalType(FastNoiseLite::FractalType_FBm);
        m_TerrainNoise.SetFractalOctaves(5);

        m_MountainNoise.SetSeed(seed + 2);
        m_MountainNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_MountainNoise.SetFrequency(0.003f);
        m_MountainNoise.SetFractalType(FastNoiseLite::FractalType_Ridged);
        m_MountainNoise.SetFractalOctaves(6);

        m_WarpNoise.SetSeed(seed + 3);
        m_WarpNoise.SetFrequency(0.005f);
        m_WarpNoise.SetDomainWarpAmp(35.0f);

        // --- Biome Noise ---
        m_BiomeNoise.SetSeed(seed + 4);
        m_BiomeNoise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        m_BiomeNoise.SetFrequency(0.0015f);
    }

    void generateChunkData(Chunk& chunk) {
        const int baseHeight = 64;
        const int waterLevel = baseHeight;
        const int deepWaterLevel = baseHeight - 12;

        auto lerp = [](float a, float b, float t) {
            return a + t * (b - a);
            };

        Biome biomeMap[CHUNK_WIDTH][CHUNK_DEPTH];

        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            for (int z = 0; z < CHUNK_DEPTH; ++z) {
                float worldX = (float)(chunk.m_Position.x * CHUNK_WIDTH + x);
                float worldZ = (float)(chunk.m_Position.z * CHUNK_DEPTH + z);

                // --- BIOME AND TERRAIN VALUES ---
                float continentalness = (m_ContinentalnessNoise.GetNoise(worldX, worldZ) + 1.0f) / 2.0f;
                float biomeValue = (m_BiomeNoise.GetNoise(worldX, worldZ) + 1.0f) / 2.0f;

                float warpX = worldX;
                float warpZ = worldZ;
                m_WarpNoise.DomainWarp(warpX, warpZ);

                float baseTerrain = (m_TerrainNoise.GetNoise(warpX, warpZ) + 1.0f) / 2.0f;
                float mountains = (m_MountainNoise.GetNoise(warpX, warpZ) + 1.0f) / 2.0f;

                // --- HEIGHT CALCULATIONS FOR EACH BIOME TYPE ---
                float plainsHeightNoise = pow(baseTerrain, 1.5f) * 0.9f;
                float forestMountainBlend = std::max(baseTerrain, mountains * 1.2f);
                float forestHeightNoise = lerp(pow(baseTerrain, 1.5f), forestMountainBlend, std::max(0.0f, mountains - 0.1f) * 1.2f);

                // --- BIOME DETERMINATION AND BLENDING ---
                float landHeightNoise;
                Biome currentBiome;
                const float continentThreshold = 0.45f;

                if (continentalness < continentThreshold) {
                    currentBiome = Biome::Ocean;
                    landHeightNoise = 0;
                }
                else {
                    const float plainsThreshold = 0.4f;
                    const float forestThreshold = 0.6f;

                    if (biomeValue < plainsThreshold) {
                        currentBiome = Biome::Plains;
                        landHeightNoise = plainsHeightNoise;
                    }
                    else if (biomeValue > forestThreshold) {
                        currentBiome = Biome::Forest;
                        landHeightNoise = forestHeightNoise;
                    }
                    else {
                        currentBiome = biomeValue < 0.5f ? Biome::Plains : Biome::Forest;
                        float blendFactor = (biomeValue - plainsThreshold) / (forestThreshold - plainsThreshold);
                        landHeightNoise = lerp(plainsHeightNoise, forestHeightNoise, blendFactor);
                    }
                }
                biomeMap[x][z] = currentBiome;

                // --- FINAL HEIGHT ASSEMBLY ---
                int landHeight = waterLevel + static_cast<int>(landHeightNoise * (CHUNK_HEIGHT - waterLevel - 5));
                int seaFloorHeight = deepWaterLevel + static_cast<int>(baseTerrain * (waterLevel - deepWaterLevel));

                int terrainHeight;
                if (currentBiome == Biome::Ocean) {
                    terrainHeight = seaFloorHeight;
                }
                else {
                    float blendFactor = std::min(1.0f, (continentalness - continentThreshold) / 0.1f);
                    terrainHeight = static_cast<int>(lerp((float)seaFloorHeight, (float)landHeight, blendFactor));
                }

                terrainHeight = std::max(1, std::min(CHUNK_HEIGHT - 1, terrainHeight));

                for (int y = 0; y < CHUNK_HEIGHT; ++y) {
                    int worldY = chunk.m_Position.y * CHUNK_HEIGHT + y;
                    if (worldY > terrainHeight) {
                        if (worldY <= waterLevel) {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Stone); // Water
                        }
                        else {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Air);
                        }
                    }
                    else if (worldY == terrainHeight) {
                        if (worldY < waterLevel + 2 && worldY >= waterLevel) {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Dirt); // Beach
                        }
                        else {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Grass);
                        }
                    }
                    else { // Below surface
                        if (worldY > terrainHeight - 4) {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Dirt);
                        }
                        else {
                            chunk.setBlock(x, y, z, (unsigned char)BlockID::Stone);
                        }
                    }
                }
            }
        }

        // --- POST-PROCESSING ---
        // Trees
        for (int x = 2; x < CHUNK_WIDTH - 2; ++x) {
            for (int z = 2; z < CHUNK_DEPTH - 2; ++z) {
                int y = CHUNK_HEIGHT - 1;
                for (; y >= 0; --y) {
                    if (chunk.getBlock(x, y, z) != (unsigned char)BlockID::Air) break;
                }
                if (y < 0) continue;

                if ((BlockID)chunk.getBlock(x, y, z) == BlockID::Grass) {
                    int worldX = chunk.m_Position.x * CHUNK_WIDTH + x;
                    int worldZ = chunk.m_Position.z * CHUNK_DEPTH + z;
                    unsigned int hash = (worldX * 18397) ^ (worldZ * 38183) ^ m_Seed;

                    Biome currentBiome = biomeMap[x][z];
                    bool placeTree = false;
                    switch (currentBiome) {
                    case Biome::Forest:
                        if ((hash % 100) < 6) placeTree = true;
                        break;
                    case Biome::Plains:
                        if ((hash % 500) < 1) placeTree = true;
                        break;
                    default:
                        break;
                    }

                    if (placeTree) {
                        generateTree(chunk, x, y + 1, z);
                    }
                }
            }
        }

        // Bedrock
        if (chunk.m_Position.y == 0) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                for (int z = 0; z < CHUNK_DEPTH; ++z) {
                    chunk.setBlock(x, 0, z, (unsigned char)BlockID::Bedrock);
                }
            }
        }
    }

private:
    void generateTree(Chunk& chunk, int x, int y, int z) {
        int height = 4 + (rand() % 3);
        if (y + height + 2 >= CHUNK_HEIGHT) return;

        for (int i = 1; i < height + 2; ++i) {
            if (chunk.getBlock(x, y + i, z) != 0) return;
        }

        chunk.setBlock(x, y - 1, z, (unsigned char)BlockID::Dirt);

        for (int i = 0; i < height; ++i) {
            chunk.setBlock(x, y + i, z, (unsigned char)BlockID::OakLog);
        }

        auto placeLeaf = [&](int lx, int ly, int lz) {
            if (lx < 0 || lx >= CHUNK_WIDTH || lz < 0 || lz >= CHUNK_DEPTH || ly < 0 || ly >= CHUNK_HEIGHT) return;
            if (chunk.getBlock(lx, ly, lz) == (unsigned char)BlockID::Air) {
                chunk.setBlock(lx, ly, lz, (unsigned char)BlockID::OakLeaves);
            }
            };

        for (int ly = y + height - 2; ly <= y + height - 1; ++ly) {
            for (int dx = -2; dx <= 2; ++dx) {
                for (int dz = -2; dz <= 2; ++dz) {
                    if (abs(dx) == 2 && abs(dz) == 2) continue;
                    if (dx == 0 && dz == 0) continue;
                    placeLeaf(x + dx, ly, z + dz);
                }
            }
        }

        int top_y = y + height;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                placeLeaf(x + dx, top_y, z + dz);
            }
        }

        int vtop_y = y + height + 1;
        placeLeaf(x, vtop_y, z);
        placeLeaf(x + 1, vtop_y, z);
        placeLeaf(x - 1, vtop_y, z);
        placeLeaf(x, vtop_y, z + 1);
        placeLeaf(x, vtop_y, z - 1);
    }

    FastNoiseLite m_ContinentalnessNoise;
    FastNoiseLite m_TerrainNoise;
    FastNoiseLite m_MountainNoise;
    FastNoiseLite m_WarpNoise;
    FastNoiseLite m_BiomeNoise;
    int m_Seed;
};