#include "World.h"
#include "Mesher.h"
#include "Frustum.h"
#include "Block.h"
#include <iostream>
#include <cstring>
#include <queue>
#include <algorithm>
#include <vector>

World::World() : m_LastPlayerChunkPos(9999, 0, 9999), m_IsRunning(true) {
    m_TerrainGenerator = std::make_unique<TerrainGenerator>(1337);
    m_SimpleMesher = std::make_unique<SimpleMesher>();
    m_GreedyMesher = std::make_unique<GreedyMesher>();

    unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned int i = 0; i < num_threads; ++i) {
        m_MesherThreads.emplace_back(&World::mesherLoop, this);
    }
    m_LightThread = std::thread(&World::lightingLoop, this);
    std::cout << "Started " << num_threads << " mesher threads and 1 lighting thread." << std::endl;
}

World::~World() {
    stopThreads();
}

void World::stopThreads() {
    if (m_IsRunning) {
        m_IsRunning = false;
        m_MeshingQueue.stop();
        m_LightUpdateQueue.stop();
        m_InitialLightQueue.stop();

        for (auto& thread : m_MesherThreads) {
            if (thread.joinable()) thread.join();
        }
        if (m_LightThread.joinable()) m_LightThread.join();
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
                m_InitialLightQueue.push(pos);
            }
        }
    }
}

void World::buildDirtyChunks() {
    std::lock_guard<std::mutex> lock(m_DirtyChunksMutex);
    if (m_DirtyChunks.empty()) return;

    for (const auto& pos : m_DirtyChunks) {
        std::lock_guard<std::mutex> jobLock(m_MeshingJobsMutex);
        if (m_MeshingJobs.find(pos) == m_MeshingJobs.end()) {
            m_MeshingJobs.insert(pos);
            m_MeshingQueue.push(pos);
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
            it->second->m_Mesh->vertices = std::move(finishedMesh.vertices);
            it->second->m_Mesh->indices = std::move(finishedMesh.indices);
            it->second->m_Mesh->upload();
            it->second->m_TransparentMesh->vertices = std::move(finishedMesh.transparentVertices);
            it->second->m_TransparentMesh->indices = std::move(finishedMesh.transparentIndices);
            it->second->m_TransparentMesh->upload();
        }

        std::lock_guard<std::mutex> jobLock(m_MeshingJobsMutex);
        m_MeshingJobs.erase(chunkPosition);
    }
}

void World::mesherLoop() {
    while (m_IsRunning) {
        glm::ivec3 jobPos;
        m_MeshingQueue.wait_and_pop(jobPos);

        if (!m_IsRunning) break;

        ChunkMeshingData dataProvider(*this, jobPos);

        IMesher* mesher = (m_UseGreedyMesher && !m_SmoothLighting)
            ? (IMesher*)m_GreedyMesher.get()
            : (IMesher*)m_SimpleMesher.get();

        Mesh tempOpaqueMesh;
        Mesh tempTransparentMesh;
        mesher->generateMesh(dataProvider, jobPos, tempOpaqueMesh, tempTransparentMesh, m_SmoothLighting);

        MeshData meshData;
        meshData.chunkPosition = jobPos;
        meshData.vertices = std::move(tempOpaqueMesh.vertices);
        meshData.indices = std::move(tempOpaqueMesh.indices);
        meshData.transparentVertices = std::move(tempTransparentMesh.vertices);
        meshData.transparentIndices = std::move(tempTransparentMesh.indices);
        m_FinishedMeshesQueue.push(std::move(meshData));
    }
}

void World::lightingLoop() {
    while (m_IsRunning) {
        LightUpdateJob job;
        if (m_LightUpdateQueue.try_pop(job)) {
            processLightUpdates(job);
        }

        glm::ivec3 initialPos;
        if (m_InitialLightQueue.try_pop(initialPos)) {
            std::shared_ptr<Chunk> chunk;
            {
                std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
                auto it = m_Chunks.find(initialPos);
                if (it != m_Chunks.end()) {
                    chunk = it->second;
                }
            }

            if (chunk) {
                propagateInitialLight(*chunk);

                const glm::ivec3 offsets[] = { {0,0,0}, {1,0,0}, {-1,0,0}, {0,0,1}, {0,0,-1} };
                {
                    std::lock_guard<std::mutex> dirtyLock(m_DirtyChunksMutex);
                    for (const auto& offset : offsets) {
                        m_DirtyChunks.insert(initialPos + offset);
                    }
                }
            }
        }
    }
}

void World::propagateInitialLight(Chunk& chunk) {
    std::queue<LightUpdateNode> sunQueue;
    std::queue<LightUpdateNode> blockQueue;

    glm::ivec3 chunkWorldPos = chunk.m_Position * glm::ivec3(CHUNK_WIDTH, 0, CHUNK_DEPTH);

    for (int x = 0; x < CHUNK_WIDTH; ++x) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            bool skyVisible = true;
            for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
                BlockID currentBlock = (BlockID)chunk.getBlock(x, y, z);
                if (skyVisible) {
                    if (BlockDataManager::isTransparentForLighting(currentBlock)) {
                        chunk.setSunlight(x, y, z, 15);
                        sunQueue.push({ chunkWorldPos + glm::ivec3(x, y, z), 15 });
                    }
                    else {
                        chunk.setSunlight(x, y, z, 0);
                        skyVisible = false;
                    }
                }
                else {
                    chunk.setSunlight(x, y, z, 0);
                }

                const auto& blockData = BlockDataManager::getData(currentBlock);
                if (blockData.emissionStrength > 0) {
                    chunk.setBlockLight(x, y, z, blockData.emissionStrength);
                    blockQueue.push({ chunkWorldPos + glm::ivec3(x, y, z), blockData.emissionStrength });
                }
                else {
                    chunk.setBlockLight(x, y, z, 0);
                }
            }
        }
    }

    while (!sunQueue.empty()) {
        LightUpdateNode node = sunQueue.front();
        sunQueue.pop();
        if (node.level <= 1) continue;

        for (const auto& offset : { glm::ivec3(0,-1,0), glm::ivec3(0,1,0), glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
            glm::ivec3 nPos = node.pos + offset;
            bool isDownward = offset.y == -1;
            unsigned char propagatedLight = (isDownward && node.level == 15) ? 15 : node.level - 1;

            if (propagatedLight > 0 && BlockDataManager::isTransparentForLighting((BlockID)getBlock(nPos.x, nPos.y, nPos.z)) && getSunlight(nPos.x, nPos.y, nPos.z) < propagatedLight) {
                setSunlight(nPos.x, nPos.y, nPos.z, propagatedLight);
                sunQueue.push({ nPos, propagatedLight });
            }
        }
    }

    while (!blockQueue.empty()) {
        LightUpdateNode node = blockQueue.front();
        blockQueue.pop();
        if (node.level <= 1) continue;

        for (const auto& offset : { glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
            glm::ivec3 nPos = node.pos + offset;
            if (BlockDataManager::isTransparentForLighting((BlockID)getBlock(nPos.x, nPos.y, nPos.z)) && getBlockLight(nPos.x, nPos.y, nPos.z) < node.level - 1) {
                setBlockLight(nPos.x, nPos.y, nPos.z, node.level - 1);
                blockQueue.push({ nPos, (unsigned char)(node.level - 1) });
            }
        }
    }
}

void World::processLightUpdates(const LightUpdateJob& job) {
    std::set<glm::ivec3, ivec3_comp> dirtyChunks;
    const auto& oldData = BlockDataManager::getData(job.oldBlock);
    const auto& newData = BlockDataManager::getData(job.newBlock);

    {
        std::queue<LightUpdateNode> removalQueue;
        std::queue<LightUpdateNode> propagationQueue;

        unsigned char lightAtPos = getBlockLight(job.pos.x, job.pos.y, job.pos.z);
        unsigned char emissionAtPos = oldData.emissionStrength;

        if (emissionAtPos > 0) {
            removalQueue.push({ job.pos, emissionAtPos });
            if (newData.emissionStrength == 0) {
                setBlockLight(job.pos.x, job.pos.y, job.pos.z, 0);
            }
        }
        else if (!BlockDataManager::isTransparentForLighting(newData.id) && lightAtPos > 0) {
            removalQueue.push({ job.pos, lightAtPos });
            setBlockLight(job.pos.x, job.pos.y, job.pos.z, 0);
        }

        if (newData.emissionStrength > 0) {
            setBlockLight(job.pos.x, job.pos.y, job.pos.z, newData.emissionStrength);
            propagationQueue.push({ job.pos, newData.emissionStrength });
        }

        if (BlockDataManager::isTransparentForLighting(newData.id) && !BlockDataManager::isTransparentForLighting(oldData.id) && oldData.emissionStrength == 0) {
            for (const auto& offset : { glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = job.pos + offset;
                unsigned char light = getBlockLight(nPos.x, nPos.y, nPos.z);
                if (light > 0) {
                    propagationQueue.push({ nPos, light });
                }
            }
        }

        while (!removalQueue.empty()) {
            LightUpdateNode node = removalQueue.front();
            removalQueue.pop();
            dirtyChunks.insert({ (int)floor((float)node.pos.x / CHUNK_WIDTH), 0, (int)floor((float)node.pos.z / CHUNK_DEPTH) });

            for (const auto& offset : { glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = node.pos + offset;
                unsigned char neighborLevel = getBlockLight(nPos.x, nPos.y, nPos.z);
                if (neighborLevel != 0) {
                    if (neighborLevel < node.level) {
                        setBlockLight(nPos.x, nPos.y, nPos.z, 0);
                        removalQueue.push({ nPos, neighborLevel });
                    }
                    else {
                        propagationQueue.push({ nPos, neighborLevel });
                    }
                }
            }
        }

        while (!propagationQueue.empty()) {
            LightUpdateNode node = propagationQueue.front();
            propagationQueue.pop();
            dirtyChunks.insert({ (int)floor((float)node.pos.x / CHUNK_WIDTH), 0, (int)floor((float)node.pos.z / CHUNK_DEPTH) });
            if (node.level <= 1) continue;

            for (const auto& offset : { glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,1,0), glm::ivec3(0,-1,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = node.pos + offset;
                if (BlockDataManager::isTransparentForLighting((BlockID)getBlock(nPos.x, nPos.y, nPos.z)) && getBlockLight(nPos.x, nPos.y, nPos.z) < node.level - 1) {
                    setBlockLight(nPos.x, nPos.y, nPos.z, node.level - 1);
                    propagationQueue.push({ nPos, (unsigned char)(node.level - 1) });
                }
            }
        }
    }

    {
        std::queue<LightUpdateNode> sunRemovalQueue, sunPropagationQueue;
        unsigned char sunAtPos = getSunlight(job.pos.x, job.pos.y, job.pos.z);

        if (!BlockDataManager::isTransparentForLighting(newData.id) && sunAtPos > 0) {
            setSunlight(job.pos.x, job.pos.y, job.pos.z, 0);
            sunRemovalQueue.push({ job.pos, sunAtPos });
        }
        else if (BlockDataManager::isTransparentForLighting(newData.id)) {
            for (const auto& offset : { glm::ivec3(0,-1,0), glm::ivec3(0,1,0), glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = job.pos + offset;
                unsigned char light = getSunlight(nPos.x, nPos.y, nPos.z);
                if (light > 0) sunPropagationQueue.push({ nPos, light });
            }
        }

        while (!sunRemovalQueue.empty()) {
            LightUpdateNode node = sunRemovalQueue.front();
            sunRemovalQueue.pop();
            dirtyChunks.insert({ (int)floor((float)node.pos.x / CHUNK_WIDTH), 0, (int)floor((float)node.pos.z / CHUNK_DEPTH) });

            for (const auto& offset : { glm::ivec3(0,-1,0), glm::ivec3(0,1,0), glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = node.pos + offset;
                unsigned char neighborLevel = getSunlight(nPos.x, nPos.y, nPos.z);
                if (neighborLevel > 0) {
                    if (neighborLevel < node.level || (offset.y == -1 && node.level == 15)) {
                        setSunlight(nPos.x, nPos.y, nPos.z, 0);
                        sunRemovalQueue.push({ nPos, neighborLevel });
                    }
                    else {
                        sunPropagationQueue.push({ nPos, neighborLevel });
                    }
                }
            }
        }

        while (!sunPropagationQueue.empty()) {
            LightUpdateNode node = sunPropagationQueue.front();
            sunPropagationQueue.pop();
            dirtyChunks.insert({ (int)floor((float)node.pos.x / CHUNK_WIDTH), 0, (int)floor((float)node.pos.z / CHUNK_DEPTH) });
            if (node.level <= 1) continue;

            for (const auto& offset : { glm::ivec3(0,-1,0), glm::ivec3(0,1,0), glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
                glm::ivec3 nPos = node.pos + offset;
                bool isDownward = offset.y == -1;
                unsigned char propagatedLight = (isDownward && node.level == 15) ? 15 : node.level - 1;

                if (propagatedLight > 0 && BlockDataManager::isTransparentForLighting((BlockID)getBlock(nPos.x, nPos.y, nPos.z)) && getSunlight(nPos.x, nPos.y, nPos.z) < propagatedLight) {
                    setSunlight(nPos.x, nPos.y, nPos.z, propagatedLight);
                    sunPropagationQueue.push({ nPos, propagatedLight });
                }
            }
        }
    }


    std::lock_guard<std::mutex> lock(m_DirtyChunksMutex);
    for (const auto& chunkPos : dirtyChunks) {
        m_DirtyChunks.insert(chunkPos);
        for (const auto& offset : { glm::ivec3(1,0,0), glm::ivec3(-1,0,0), glm::ivec3(0,0,1), glm::ivec3(0,0,-1) }) {
            m_DirtyChunks.insert(chunkPos + offset);
        }
    }
}

int World::renderOpaque(Shader& shader, const Frustum& frustum) {
    int chunksRendered = 0;
    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    for (auto const& [pos, chunk] : m_Chunks) {
        glm::vec3 min(pos.x * CHUNK_WIDTH, pos.y * CHUNK_HEIGHT, pos.z * CHUNK_DEPTH);
        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);

        if (frustum.isBoxInFrustum(min, max)) {
            chunk->drawOpaque();
            chunksRendered++;
        }
    }
    return chunksRendered;
}

void World::renderTransparent(Shader& shader, const Frustum& frustum) {
    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    for (auto const& [pos, chunk] : m_Chunks) {
        glm::vec3 min(pos.x * CHUNK_WIDTH, pos.y * CHUNK_HEIGHT, pos.z * CHUNK_DEPTH);
        glm::vec3 max = min + glm::vec3(CHUNK_WIDTH, CHUNK_HEIGHT, CHUNK_DEPTH);

        if (frustum.isBoxInFrustum(min, max)) {
            chunk->drawTransparent();
        }
    }
}

unsigned char World::getBlock(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, 0, chunkZ });

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

    BlockID oldBlockId;

    {
        std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
        auto it = m_Chunks.find(targetChunkPos);
        if (it == m_Chunks.end()) return;

        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;

        oldBlockId = (BlockID)it->second->getBlock(localX, y, localZ);
        if (blockId == oldBlockId) return;

        it->second->setBlock(localX, y, localZ, static_cast<unsigned char>(blockId));
    }

    {
        std::lock_guard<std::mutex> lock(m_DirtyChunksMutex);
        m_DirtyChunks.insert(targetChunkPos);
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        if (localX == 0) m_DirtyChunks.insert({ chunkX - 1, 0, chunkZ });
        if (localX == CHUNK_WIDTH - 1) m_DirtyChunks.insert({ chunkX + 1, 0, chunkZ });
        if (localZ == 0) m_DirtyChunks.insert({ chunkX, 0, chunkZ - 1 });
        if (localZ == CHUNK_DEPTH - 1) m_DirtyChunks.insert({ chunkX, 0, chunkZ + 1 });
    }

    m_LightUpdateQueue.push({ {x, y, z}, oldBlockId, blockId });
}

unsigned char World::getSunlight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 15;
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, 0, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return it->second->getSunlight(localX, localY, localZ);
    }
    return 15;
}

void World::setSunlight(int x, int y, int z, unsigned char level) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, 0, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        it->second->setSunlight(localX, localY, localZ, level);
    }
}

unsigned char World::getBlockLight(int x, int y, int z) const {
    if (y < 0 || y >= CHUNK_HEIGHT) return 0;
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, 0, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        return it->second->getBlockLight(localX, localY, localZ);
    }
    return 0;
}

void World::setBlockLight(int x, int y, int z, unsigned char level) {
    if (y < 0 || y >= CHUNK_HEIGHT) return;
    int chunkX = static_cast<int>(floor((float)x / CHUNK_WIDTH));
    int chunkZ = static_cast<int>(floor((float)z / CHUNK_DEPTH));

    std::shared_lock<std::shared_mutex> lock(m_ChunksMutex);
    auto it = m_Chunks.find({ chunkX, 0, chunkZ });

    if (it != m_Chunks.end()) {
        int localX = (x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
        int localY = (y % CHUNK_HEIGHT + CHUNK_HEIGHT) % CHUNK_HEIGHT;
        int localZ = (z % CHUNK_DEPTH + CHUNK_DEPTH) % CHUNK_DEPTH;
        it->second->setBlockLight(localX, localY, localZ, level);
    }
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
    m_LastPlayerChunkPos = glm::ivec3(9999, 0, 9999);
}