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
#include "Mesher.h"
#include "Block.h"
#include "GraphicsSettings.h"

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
    std::vector<float> transparentVertices;
    std::vector<unsigned int> transparentIndices;
};

struct LightUpdateNode {
    glm::ivec3 pos;
    unsigned char level;
};

struct LightUpdateJob {
    glm::ivec3 pos;
    BlockID oldBlock;
    BlockID newBlock;
};

class ChunkMeshingData;
class Frustum;

class World {
    friend class ChunkMeshingData;

public:
    int m_RenderDistance = 12;
    bool m_UseGreedyMesher = false;
    bool m_UseSunlight = true;
    bool m_SmoothLighting = true;
    std::atomic<LeafQuality> m_LeafQuality{ LeafQuality::Fancy };

    World();
    ~World();
    void update(const glm::vec3& playerPosition);
    int renderOpaque(Shader& shader, const Frustum& frustum);
    void renderTransparent(Shader& shader, const Frustum& frustum);
    unsigned char getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockID blockId);
    unsigned char getSunlight(int x, int y, int z) const;
    void setSunlight(int x, int y, int z, unsigned char level);
    unsigned char getBlockLight(int x, int y, int z) const;
    void setBlockLight(int x, int y, int z, unsigned char level);

    size_t getChunkCount() const;
    void forceReload();
    void stopThreads();

private:
    void loadChunks(const glm::ivec3& playerChunkPos);
    void buildDirtyChunks();
    void processFinishedMeshes();
    void mesherLoop();
    void lightingLoop();

    void propagateInitialLight(Chunk& chunk);
    void processLightUpdates(const LightUpdateJob& job);

    std::map<glm::ivec3, std::shared_ptr<Chunk>, ivec3_comp> m_Chunks;
    std::unique_ptr<TerrainGenerator> m_TerrainGenerator;
    std::unique_ptr<SimpleMesher> m_SimpleMesher;
    std::unique_ptr<GreedyMesher> m_GreedyMesher;

    glm::ivec3 m_LastPlayerChunkPos;
    std::set<glm::ivec3, ivec3_comp> m_DirtyChunks;
    std::mutex m_DirtyChunksMutex;

    std::vector<std::thread> m_MesherThreads;
    std::thread m_LightThread;

    ThreadSafeQueue<glm::ivec3> m_MeshingQueue;
    ThreadSafeQueue<MeshData> m_FinishedMeshesQueue;
    ThreadSafeQueue<LightUpdateJob> m_LightUpdateQueue;
    ThreadSafeQueue<glm::ivec3> m_InitialLightQueue;

    std::atomic<bool> m_IsRunning;
    mutable std::shared_mutex m_ChunksMutex;

    std::set<glm::ivec3, ivec3_comp> m_MeshingJobs;
    std::mutex m_MeshingJobsMutex;
};