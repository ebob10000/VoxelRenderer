#pragma once
#include "Chunk.h"
#include "Mesh.h"

// Forward declare to avoid circular dependencies
class World;
class Chunk;
struct Mesh;

// The interface for all meshing algorithms
class IMesher {
public:
    virtual void generateMesh(Chunk& chunk, World& world, Mesh& mesh) = 0;
};

// --- Implementation for the Simple Mesher ---
class SimpleMesher : public IMesher {
public:
    void generateMesh(Chunk& chunk, World& world, Mesh& mesh) override;
};

// --- Implementation for the Greedy Mesher ---
class GreedyMesher : public IMesher {
public:
    void generateMesh(Chunk& chunk, World& world, Mesh& mesh) override;
};