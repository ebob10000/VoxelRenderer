#include "World.h"
#include "Mesher.h"
#include <iostream>
#include <cstring>

World::World() : m_LastPlayerChunkPos(9999), m_IsRunning(true) {
    m_TerrainGenerator = std::make_unique<TerrainGenerator>(1337);
    m_SimpleMesher = std::make_unique<SimpleMesher>();
    m_GreedyMesher = std::make_unique<GreedyMesher>();

    unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned int i = 0; i < num_threads; ++i) {
        m_WorkerThreads.emplace_back(&World::workerLoop, this);
    }
    std::cout << "Started " << num_threads << " worker threads." << std::endl;
}

World::~World() {
    stopThreads();
}

void World::stopThreads() {
    if (m_IsRunning) {
        m_IsRunning = false;
        m_GenerationQueue.stop();
        for (auto& thread : m_WorkerThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
}

void World::update(const glm::vec3& playerPosition) {
    glm::ivec3 playerChunkPos(
        static_cast<int>(floor(playerPosition.x / CHUNK_WIDTH)),
        0,
        static_cast<int>(floor(playerPosition.z / CHUNK_DEPTH))
    );

    if (playerChunkPos != m_LastPlayerChunkPos) {
        loadChunks(playerChunkPos);
        m_LastPlayerChunkPos = playerChunkPos;
    }

    buildDirtyChunks();
    processFinishedMeshes();
}

void World::calculateSunlight(Chunk& chunk) {
    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            unsigned char lightLevel = 15;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                if (chunk.getBlock(x, y, z) != 0) {
                    lightLevel = 0;
                }
                chunk.setLight(x, y, z, lightLevel);
            }
        }
    }
}

void World::loadChunks(const glm::ivec3& playerChunkPos) {
    std::vector<glm::ivec3> toLoad;
    std::vector<glm::ivec3> toUnload;

    {
        std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
        for (auto const& [pos, chunk] : m_Chunks) {
            if (abs(pos.x - playerChunkPos.x) > m_RenderDistance ||
                abs(pos.z - playerChunkPos.z) > m_RenderDistance) {
                toUnload.push_back(pos);
            }
        }
    }

    for (int x = playerChunkPos.x - m_RenderDistance; x <= playerChunkPos.x + m_RenderDistance; ++x) {
        for (int z = playerChunkPos.z - m_RenderDistance; z <= playerChunkPos.z + m_RenderDistance; ++z) {
            toLoad.push_back({ x, 0, z });
        }
    }

    {
        std::unique_lock<std::shared_mutex> lock(m_ChunksMutex);
        for (const auto& pos : toUnload) {
            m_Chunks.erase(pos);
        }

        for (const auto& pos : toLoad) {
            if (m_Chunks.find(pos) == m_Chunks.end()) {
                auto newChunk = std::make_unique<Chunk>(pos.x, pos.y, pos.z);
                m_TerrainGenerator->generateChunkData(*newChunk);
                calculateSunlight(*newChunk);
                m_Chunks[pos] = std::move(newChunk);

                m_DirtyChunks.insert(pos);
                m_DirtyChunks.insert({ pos.x + 1, 0, pos.z });
                m_DirtyChunks.insert({ pos.x - 1, 0, pos.z });
                m_DirtyChunks.insert({ pos.x, 0, pos.z + 1 });
                m_DirtyChunks.insert({ pos.x, 0, pos.z - 1 });
            }
        }
    }
}

void World::buildDirtyChunks() {
    if (m_DirtyChunks.empty()) return;

    for (const auto& pos : m_DirtyChunks) {
        bool alreadyProcessing;
        {
            std::lock_guard<std::mutex> lock(m_MeshingJobsMutex);
            alreadyProcessing = m_MeshingJobs.count(pos);
        }

        if (!alreadyProcessing) {
            std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
            auto it = m_Chunks.find(pos);
            if (it != m_Chunks.end()) {
                ChunkGenerationData data;
                data.position = pos;
                // We no longer need to copy data here, the provider will access it.

                {
                    std::lock_guard<std::mutex> jobLock(m_MeshingJobsMutex);
                    m_MeshingJobs.insert(pos);
                }
                m_GenerationQueue.push(std::move(data));
            }
        }
    }
    m_DirtyChunks.clear();
}

void World::processFinishedMeshes() {
    MeshData finishedMesh;
    while (m_FinishedMeshesQueue.try_pop(finishedMesh)) {
        std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find(finishedMesh.chunkPosition);
        if (it != m_Chunks.end()) {
            it->second->m_Mesh->vertices = std::move(finishedMesh.vertices);
            it->second->m_Mesh->indices = std::move(finishedMesh.indices);
            it->second->m_Mesh->upload();
        }
    }
}

void World::workerLoop() {
    while (m_IsRunning) {
        ChunkGenerationData jobData;
        m_GenerationQueue.wait_and_pop(jobData);

        if (!m_IsRunning) break;

        // The critical optimization: gather all necessary chunk data ONCE with a single lock.
        ChunkMeshingData dataProvider(*this, jobData.position);

        IMesher* mesher = m_UseGreedyMesher ? (IMesher*)m_GreedyMesher.get() : (IMesher*)m_SimpleMesher.get();

        Mesh tempMesh;
        // The mesher now operates on the fast, local data with no connection to the World.
        mesher->generateMesh(dataProvider, jobData.position, tempMesh);

        if (!tempMesh.vertices.empty()) {
            MeshData meshData;
            meshData.chunkPosition = jobData.position;
            meshData.vertices = std::move(tempMesh.vertices);
            meshData.indices = std::move(tempMesh.indices);
            m_FinishedMeshesQueue.push(std::move(meshData));
        }

        {
            std::lock_guard<std::mutex> lock(m_MeshingJobsMutex);
            m_MeshingJobs.erase(jobData.position);
        }
    }
}

void World::render(Shader& shader) {
    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    for (auto const& [pos, chunk] : m_Chunks) {
        chunk->draw();
    }
}

unsigned char World::getBlock(int x, int y, int z) const {
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkY = static_cast<int>(floor((float)y / CHUNK_HEIGHT));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, chunkY, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return it->second->getBlock(localX, localY, localZ);
    }
    return 0;
}

unsigned char World::getLight(int x, int y, int z) const {
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkY = static_cast<int>(floor((float)y / CHUNK_HEIGHT));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, chunkY, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return it->second->getLight(localX, localY, localZ);
    }
    return 15;
}


size_t World::getChunkCount() const {
    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    return m_Chunks.size();
}

void World::forceReload() {
    {
        std::unique_lock<std::shared_mutex> lock(m_ChunksMutex);
        m_Chunks.clear();
    }
    m_LastPlayerChunkPos = glm::ivec3(9999);
}