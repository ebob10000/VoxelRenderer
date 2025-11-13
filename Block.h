#pragma once
#include <glm/glm.hpp>
#include <map>
#include "GraphicsSettings.h"

enum class BlockID : unsigned char {
    Air = 0,
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Glowstone = 4,
    Bedrock = 5,
    OakLog = 6,
    OakLeaves = 7
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

    static inline bool isTransparentForLighting(BlockID id) {
        return id == BlockID::Air || id == BlockID::OakLeaves;
    }

    static inline bool shouldRenderFace(BlockID currentBlockID, BlockID neighborBlockID, LeafQuality quality) {
        if (neighborBlockID == BlockID::Air) {
            return true;
        }

        bool currentIsLeaves = (currentBlockID == BlockID::OakLeaves);
        bool neighborIsLeaves = (neighborBlockID == BlockID::OakLeaves);

        if (currentIsLeaves) {
            if (neighborIsLeaves) {
                return quality == LeafQuality::Fancy;
            }
            return quality == LeafQuality::Fast;
        }

        if (neighborIsLeaves) {
            return quality != LeafQuality::Fast;
        }

        return false;
    }

private:
    // Texture Indices: Grass Top(0), Grass Side(1), Dirt(2), Stone(3), Bedrock(4), Glowstone(9), Leaves(10), Log Side(11), Log Top(12)
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
        }}},
        { BlockID::OakLog, { BlockID::OakLog, {
            { {11, 15} }, // -X (Side)
            { {11, 15} }, // +X (Side)
            { {12, 15} }, // -Y (Bottom)
            { {12, 15} }, // +Y (Top)
            { {11, 15} }, // -Z (Side)
            { {11, 15} }  // +Z (Side)
        }}},
        { BlockID::OakLeaves, { BlockID::OakLeaves, {
            { {10, 15} }, { {10, 15} }, { {10, 15} }, { {10, 15} }, { {10, 15} }, { {10, 15} }
        }}}
    };
};