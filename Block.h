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
    // The texture atlas is loaded flipped vertically, so the Y-coordinate needs to be inverted.
    // The atlas is 16x16 tiles. The artist's top row (Y=0) is Y=15 in our texture coordinates.
    // Textures from left to right: Grass Top (0), Grass Side (1), Dirt (2), Stone (3)
    static const inline std::map<BlockID, BlockData> m_BlockDataMap = {
        { BlockID::Stone, { BlockID::Stone, {
            { {3, 15} }, { {3, 15} }, { {3, 15} }, { {3, 15} }, { {3, 15} }, { {3, 15} }
        }}},
        { BlockID::Dirt, { BlockID::Dirt, {
            { {2, 15} }, { {2, 15} }, { {2, 15} }, { {2, 15} }, { {2, 15} }, { {2, 15} }
        }}},
        { BlockID::Grass, { BlockID::Grass, {
            { {1, 15} }, // -X (Side is Grass Side)
            { {1, 15} }, // +X (Side is Grass Side)
            { {2, 15} }, // -Y (Bottom is Dirt)
            { {0, 15} }, // +Y (Top is Grass Top)
            { {1, 15} }, // -Z (Side is Grass Side)
            { {1, 15} }  // +Z (Side is Grass Side)
        }}}
    };
};