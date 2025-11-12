#pragma once
#include <glm/glm.hpp>
#include <map>

enum class BlockID : unsigned char {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Glowstone = 4,
    Bedrock = 5
};

struct BlockFace {
    glm::ivec2 tex_coords;
};

struct BlockData {
    BlockID id;
    BlockFace faces[6]; // 0: -X, 1: +X, 2: -Y, 3: +Y, 4: -Z, 5: +Z
    unsigned char emissionStrength = 0;
};

class BlockDataManager {
public:
    static inline const BlockData& getData(BlockID id) {
        if (id == BlockID::Air) {
            static const BlockData airData = { BlockID::Air };
            return airData;
        }
        return m_BlockDataMap.at(id);
    }

private:
    // Textures: Grass Top(0), Grass Side(1), Dirt(2), Stone(3), Bedrock(4), Glowstone(9)
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
        }}},
        { BlockID::Glowstone, { BlockID::Glowstone, {
            { {9, 15} }, { {9, 15} }, { {9, 15} }, { {9, 15} }, { {9, 15} }, { {9, 15} }
        }, 15}},
        { BlockID::Bedrock, { BlockID::Bedrock, {
            { {4, 15} }, { {4, 15} }, { {4, 15} }, { {4, 15} }, { {4, 15} }, { {4, 15} }
        }}}
    };
};