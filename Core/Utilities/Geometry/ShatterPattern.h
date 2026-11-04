#pragma once
#include <span>
#include <vector>
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/Rect.h"

// Fracture pattern of a rectangle (glass, screen): each seed's shard contains the rectangle's points nearest to it
// (Voronoi). Shards are convex and cover the rectangle without gaps; an edge shared by two shards is a crack,
// while rectangle edges are not cracks. Pure geometry; drawing and timed crack propagation belong to the caller.
struct ShatterShard {
    std::vector<Float2> outline; // Convex, corners in order; empty for a seed outside the rectangle or a duplicate
    Float2 seed;
    Float2 center; // Centroid
    float area = 0.0f;
};

struct ShatterCrack {
    Float2 from;
    Float2 to;
    float length = 0.0f;
    int left = 0; // Indices of the two shards it separates
    int right = 0;
};

class ShatterPattern {
public:
    // Shard indices match seed indices.
    ShatterPattern(const Rect& bounds, std::span<const Float2> seeds);

    [[nodiscard]] const std::vector<ShatterShard>& Shards() const { return _shards; }
    [[nodiscard]] const std::vector<ShatterCrack>& Cracks() const { return _cracks; }
    // Shard containing 'point' (the nearest seed's); -1 if there are no shards.
    [[nodiscard]] int ShardAt(Float2 point) const;

private:
    std::vector<ShatterShard> _shards;
    std::vector<ShatterCrack> _cracks;
};
