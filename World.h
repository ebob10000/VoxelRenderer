#pragma once
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Chunk.h"
#include "TerrainGenerator.h"
#include "ThreadSafeQueue.h"
#include "Mesher.h" // <<< THE FIX IS HERE

struct ivec3_comp {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};

struct MeshData {
    glm::ivec3 chunkPosition;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct ChunkGenerationData {
    glm::ivec3 position;
};

// Forward declare to allow friendship
class ChunkMeshingData;

class World {
    friend class ChunkMeshingData; // Grant the data provider access to private members

public:
    int m_RenderDistance = 8;
    bool m_UseGreedyMesher = false;
    bool m_UseAO = true;
    bool m_UseSunlight = true;

    World();
    ~World();
    void update(const glm::vec3& playerPosition);
    void render(Shader& shader);
    unsigned char getBlock(int x, int y, int z) const;
    unsigned char getLight(int x, int y, int z) const;
    size_t getChunkCount() const;
    void forceReload();
    void stopThreads();

private:
    void loadChunks(const glm::ivec3& playerChunkPos);
    void buildDirtyChunks();
    void processFinishedMeshes();
    void workerLoop();
    void calculateSunlight(Chunk& chunk);

    std::map<glm::ivec3, std::unique_ptr<Chunk>, ivec3_comp> m_Chunks;
    std::unique_ptr<TerrainGenerator> m_TerrainGenerator;
    std::unique_ptr<SimpleMesher> m_SimpleMesher;
    std::unique_ptr<GreedyMesher> m_GreedyMesher;

    glm::ivec3 m_LastPlayerChunkPos;
    std::set<glm::ivec3, ivec3_comp> m_DirtyChunks;

    std::vector<std::thread> m_WorkerThreads;
    ThreadSafeQueue<ChunkGenerationData> m_GenerationQueue;
    ThreadSafeQueue<MeshData> m_FinishedMeshesQueue;
    std::atomic<bool> m_IsRunning;
    mutable std::shared_mutex m_ChunksMutex;

    std::set<glm::ivec3, ivec3_comp> m_MeshingJobs;
    std::mutex m_MeshingJobsMutex;
};