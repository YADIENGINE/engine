#pragma once
#include <span>
#include <vector>
#include "Utilities/Float2.h"
#include "Utilities/Rect.h"

// Fracture pattern of a rectangle (glass, screen): each seed's shard contains the rectangle's points nearest to it
// (Voronoi). Shards are convex and cover the rectangle without gaps; an edge shared by two shards is a crack,
// while rectangle edges are not cracks. Spread gives when and from which end cracks open. Pure geometry;
// drawing belongs to the caller.
struct ShatterShard {
    std::vector<Float2> outline; // Convex, corners in order; empty for a seed outside the rectangle or a duplicate
    Float2 seed;
    Float2 center; // Centroid
    float area = 0.0f;
    float crackedAt = 0.0f; // When its first boundary crack starts opening (Spread)
    float freedAt = 0.0f;   // When all its cracks are open: the shard is free (Spread)
};

struct ShatterCrack {
    Float2 from; // After Spread: the end where the crack starts opening
    Float2 to;
    float length = 0.0f;
    int left = 0; // Indices of the two shards it separates
    int right = 0;
    float start = 0.0f; // When opening starts at 'from' (Spread)
    float end = 0.0f;   // When it reaches 'to'
};

// Source of a crack wave: starts from 'point' at 'time'.
struct CrackSource {
    Float2 point;
    float time = 0.0f;
};

class ShatterPattern {
public:
    // Shard indices match seed indices.
    ShatterPattern(const Rect& bounds, std::span<const Float2> seeds);

    // Wave spreading from sources at 'speed' (units/s, > 0): each crack starts opening at the end reached first,
    // then advances to its other end at the same speed. Without sources, all cracks are open at time 0.
    void Spread(std::span<const CrackSource> sources, float speed);

    [[nodiscard]] const std::vector<ShatterShard>& Shards() const { return _shards; }
    [[nodiscard]] const std::vector<ShatterCrack>& Cracks() const { return _cracks; }
    // Shard containing 'point' (the nearest seed's); -1 if there are no shards.
    [[nodiscard]] int ShardAt(Float2 point) const;

private:
    std::vector<ShatterShard> _shards;
    std::vector<ShatterCrack> _cracks;
};
