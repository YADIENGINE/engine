#pragma once
#include <span>
#include <vector>
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/ShatterPattern.h"

// Source of a crack wave: starts from 'point' at 'time'.
struct CrackSource {
    Float2 point;
    float time = 0.0f;
};

struct ShatterShardTiming {
    float crackedAt = 0.0f; // When its first boundary crack starts opening
    float freedAt = 0.0f;   // When all its boundary cracks are open
};

struct ShatterCrackTiming {
    Float2 from; // End reached first by the nearest source
    Float2 to;
    float start = 0.0f;
    float end = 0.0f;
};

// Timed propagation over a ShatterPattern. Shard and crack timing indices match the pattern's geometry indices.
// With no sources every crack is open at time zero. Speed is in geometry units per second and must be positive.
class ShatterSpread {
public:
    ShatterSpread(const ShatterPattern& pattern, std::span<const CrackSource> sources, float speed);

    [[nodiscard]] const std::vector<ShatterShardTiming>& Shards() const { return _shards; }
    [[nodiscard]] const std::vector<ShatterCrackTiming>& Cracks() const { return _cracks; }

private:
    std::vector<ShatterShardTiming> _shards;
    std::vector<ShatterCrackTiming> _cracks;
};
