#include "World.h"
#include "Mesher.h"
#include "Frustum.h"
#include "Block.h"
#include <iostream>
#include <cstring>
#include <queue>
#include <algorithm>
#include <vector>

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

void World::calculateSunlight(const unsigned char* blocks, unsigned char* outLight, const glm::ivec3& chunkPos) {
    const unsigned char(*blocks3D)[CHUNK_HEIGHT][CHUNK_DEPTH] = (const unsigned char(*)[CHUNK_HEIGHT][CHUNK_DEPTH])blocks;
    unsigned char(*lights3D)[CHUNK_HEIGHT][CHUNK_DEPTH] = (unsigned char(*)[CHUNK_HEIGHT][CHUNK_DEPTH])outLight;

    std::vector<glm::ivec3> lightQueue;
    lightQueue.reserve(CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT / 2);
    size_t queueIndex = 0;

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            bool blocked = false;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                if (blocks3D[x][y][z] != 0) {
                    blocked = true;
                }

                if (blocked) {
                    lights3D[x][y][z] = 0;
                }
                else {
                    lights3D[x][y][z] = 15;
                    lightQueue.emplace_back(x, y, z);
                }
            }
        }
    }

    int chunkWorldX = chunkPos.x * CHUNK_WIDTH;
    int chunkWorldY = chunkPos.y * CHUNK_HEIGHT;
    int chunkWorldZ = chunkPos.z * CHUNK_DEPTH;

    auto processFace = [&](int startX, int endX, int startZ, int endZ, int normX, int normZ) {
        for (int y = 0; y < CHUNK_HEIGHT; ++y) {
            for (int x = startX; x < endX; ++x) {
                for (int z = startZ; z < endZ; ++z) {
                    if (blocks3D[x][y][z] == 0) {
                        unsigned char neighborLight = this->getLight(chunkWorldX + x + normX, chunkWorldY + y, chunkWorldZ + z + normZ);
                        if (neighborLight > 1 && (neighborLight - 1) > lights3D[x][y][z]) {
                            lights3D[x][y][z] = neighborLight - 1;
                            lightQueue.emplace_back(x, y, z);
                        }
                    }
                }
            }
        }
        };

    processFace(0, 1, 0, CHUNK_DEPTH, -1, 0);
    processFace(CHUNK_WIDTH - 1, CHUNK_WIDTH, 0, CHUNK_DEPTH, 1, 0);
    processFace(0, CHUNK_WIDTH, 0, 1, 0, -1);
    processFace(0, CHUNK_WIDTH, CHUNK_DEPTH - 1, CHUNK_DEPTH, 0, 1);

    const glm::ivec3 offsets[] = {
        { 1, 0, 0}, {-1, 0, 0},
        { 0, 1, 0}, { 0,-1, 0},
        { 0, 0, 1}, { 0, 0,-1}
    };

    while (queueIndex < lightQueue.size()) {
        glm::ivec3 pos = lightQueue[queueIndex++];

        unsigned char currentLight = lights3D[pos.x][pos.y][pos.z];
        if (currentLight <= 1) continue;

        unsigned char propagatedLight = currentLight - 1;

        for (const auto& offset : offsets) {
            glm::ivec3 neighborPos = pos + offset;

            if (neighborPos.x < 0 || neighborPos.x >= CHUNK_WIDTH ||
                neighborPos.y < 0 || neighborPos.y >= CHUNK_HEIGHT ||
                neighborPos.z < 0 || neighborPos.z >= CHUNK_DEPTH) {
                continue;
            }

            if (blocks3D[neighborPos.x][neighborPos.y][neighborPos.z] == 0 && lights3D[neighborPos.x][neighborPos.y][neighborPos.z] < propagatedLight) {
                lights3D[neighborPos.x][neighborPos.y][neighborPos.z] = propagatedLight;
                lightQueue.push_back(neighborPos);
            }
        }
    }
}

void World::loadChunks(const glm::ivec3& playerChunkPos) {
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

    {
        std::unique_lock<std::shared_mutex> lock(m_ChunksMutex);
        for (const auto& pos : toUnload) {
            m_Chunks.erase(pos);
        }
    }

    for (int x = playerChunkPos.x - m_RenderDistance; x <= playerChunkPos.x + m_RenderDistance; ++x) {
        for (int z = playerChunkPos.z - m_RenderDistance; z <= playerChunkPos.z + m_RenderDistance; ++z) {
            glm::ivec3 pos(x, 0, z);
            bool chunkExists;
            {
                std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
                chunkExists = m_Chunks.count(pos);
            }

            if (!chunkExists) {
                auto newChunk = std::make_shared<Chunk>(pos.x, pos.y, pos.z);
                m_TerrainGenerator->generateChunkData(*newChunk);
                {
                    std::unique_lock<std::shared_mutex> lock(m_ChunksMutex);
                    m_Chunks[pos] = std::move(newChunk);
                }
                m_DirtyChunks.insert(pos);
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
            ChunkGenerationData data;
            data.position = pos;
            {
                std::lock_guard<std::mutex> jobLock(m_MeshingJobsMutex);
                m_MeshingJobs.insert(pos);
            }
            m_GenerationQueue.push(std::move(data));
        }
    }
    m_DirtyChunks.clear();
}

void World::processFinishedMeshes() {
    MeshData finishedMesh;
    while (m_FinishedMeshesQueue.try_pop(finishedMesh)) {
        glm::ivec3 chunkPosition = finishedMesh.chunkPosition;
        std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find(chunkPosition);
        if (it != m_Chunks.end()) {
            it->second->setLightLevels(finishedMesh.lightLevels.get());
            it->second->m_Mesh->vertices = std::move(finishedMesh.vertices);
            it->second->m_Mesh->indices = std::move(finishedMesh.indices);
            it->second->m_Mesh->upload();

            if (!it->second->m_HasBeenMeshed) {
                it->second->m_HasBeenMeshed = true;

                const glm::ivec3 offsets[] = { {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
                for (const auto& offset : offsets) {
                    glm::ivec3 neighborPos = chunkPosition + offset;
                    if (m_Chunks.count(neighborPos)) {
                        m_DirtyChunks.insert(neighborPos);
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_MeshingJobsMutex);
            m_MeshingJobs.erase(chunkPosition);
        }
    }
}

void World::workerLoop() {
    while (m_IsRunning) {
        ChunkGenerationData jobData;
        m_GenerationQueue.wait_and_pop(jobData);

        if (!m_IsRunning) break;

        unsigned char localBlocks[CHUNK_WIDTH][CHUNK_HEIGHT][CHUNK_DEPTH];
        {
            std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
            auto it = m_Chunks.find(jobData.position);
            if (it == m_Chunks.end()) {
                std::lock_guard<std::mutex> jobLock(m_MeshingJobsMutex);
                m_MeshingJobs.erase(jobData.position);
                continue;
            }
            std::memcpy(localBlocks, it->second->getBlocks(), sizeof(localBlocks));
        }

        auto localLightLevels = std::make_unique<unsigned char[]>(CHUNK_WIDTH * CHUNK_HEIGHT * CHUNK_DEPTH);
        calculateSunlight(&localBlocks[0][0][0], localLightLevels.get(), jobData.position);

        ChunkMeshingData dataProvider(*this, jobData.position, &localBlocks[0][0][0], localLightLevels.get());
        IMesher* mesher = m_UseGreedyMesher ? (IMesher*)m_GreedyMesher.get() : (IMesher*)m_SimpleMesher.get();

        Mesh tempMesh;
        mesher->generateMesh(dataProvider, jobData.position, tempMesh);

        MeshData meshData;
        meshData.chunkPosition = jobData.position;
        meshData.vertices = std::move(tempMesh.vertices);
        meshData.indices = std::move(tempMesh.indices);
        meshData.lightLevels = std::move(localLightLevels);
        m_FinishedMeshesQueue.push(std::move(meshData));
    }
}

int World::render(Shader& shader, const Frustum& frustum) {
    int chunksRendered = 0;
    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    for (auto const& [pos, chunk] : m_Chunks) {
        glm::vec3 min(pos.x * CHUNK_WIDTH, pos.y * CHUNK_HEIGHT, pos.z * CHUNK_DEPTH);
        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);

        if (frustum.isBoxInFrustum(min, max)) {
            chunk->draw();
            chunksRendered++;
        }
    }
    return chunksRendered;
}

unsigned char World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
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

void World::setBlock(int x, int y, int z, BlockID blockId) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;

    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));
    glm::ivec3 targetChunkPos(chunkX, 0, chunkZ);

    {
        std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find(targetChunkPos);
        if (it == m_Chunks.end()) return;

        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        it->second->setBlock(localX, y, localZ, static_cast<unsigned char>(blockId));
    }

    m_DirtyChunks.insert(targetChunkPos);

    int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
    int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;

    if (localX == 0) m_DirtyChunks.insert({ chunkX - 1, 0, chunkZ });
    if (localX == CHUNK_WIDTH - 1) m_DirtyChunks.insert({ chunkX + 1, 0, chunkZ });
    if (localZ == 0) m_DirtyChunks.insert({ chunkX, 0, chunkZ - 1 });
    if (localZ == CHUNK_DEPTH - 1) m_DirtyChunks.insert({ chunkX, 0, chunkZ + 1 });
}

unsigned char World::getLight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 15;
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