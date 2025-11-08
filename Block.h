#pragma once
#include <glm/glm.hpp>
#include <map>

// Using an enum makes block IDs clear and type-safe.
enum class BlockID : unsigned char {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3
};

// Stores the texture coordinates (in tile units) for one face of a block.
struct BlockFace {
    glm::ivec2 tex_coords;
};

// Defines all properties for a single type of block.
struct BlockData {
    BlockID id;
    BlockFace faces[6]; // 0: -X, 1: +X, 2: -Y, 3: +Y, 4: -Z, 5: +Z
};

// A static class to manage all block definitions.
class BlockDataManager {
public:
    static inline const BlockData& getData(BlockID id) {
        return m_BlockDataMap.at(id);
    }

private:
    // THE FIX: These coordinates now match the vertically-flipped atlas in memory.
    static const inline std::map<BlockID, BlockData> m_BlockDataMap = {
        { BlockID::Stone, { BlockID::Stone, {
            { {1, 0} }, { {1, 0} }, { {1, 0} }, { {1, 0} }, { {1, 0} }, { {1, 0} }
        }}},
        { BlockID::Dirt, { BlockID::Dirt, {
            { {0, 0} }, { {0, 0} }, { {0, 0} }, { {0, 0} }, { {0, 0} }, { {0, 0} }
        }}},
        { BlockID::Grass, { BlockID::Grass, {
            { {1, 1} }, // -X (Side is Grass Side)
            { {1, 1} }, // +X (Side is Grass Side)
            { {0, 0} }, // -Y (Bottom is Dirt)
            { {0, 1} }, // +Y (Top is Grass Top)
            { {1, 1} }, // -Z (Side is Grass Side)
            { {1, 1} }  // +Z (Side is Grass Side)
        }}}
    };
};