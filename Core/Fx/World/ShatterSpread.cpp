#include "Fx/World/ShatterSpread.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

ShatterSpread::ShatterSpread(const ShatterPattern& pattern, std::span<const CrackSource> sources, float speed) {
    if (!std::isfinite(speed) || speed <= 0.0f) throw std::invalid_argument("Shatter speed must be positive");

    const float pace = 1.0f / speed;
    const auto arrival = [&](Float2 point) {
        float best = std::numeric_limits<float>::max();
        for (const CrackSource& source : sources)
            best = std::min(best, source.time + Distance(point, source.point) * pace);
        return best;
    };

    _shards.resize(pattern.Shards().size(), {std::numeric_limits<float>::max(), 0.0f});
    _cracks.reserve(pattern.Cracks().size());
    for (const ShatterCrack& geometry : pattern.Cracks()) {
        ShatterCrackTiming timing{.from = geometry.from, .to = geometry.to};
        if (!sources.empty()) {
            const float from = arrival(timing.from);
            const float to = arrival(timing.to);
            if (to < from) std::swap(timing.from, timing.to);
            timing.start = std::min(from, to);
            timing.end = timing.start + geometry.length * pace;
        }
        _cracks.push_back(timing);
        for (const int index : {geometry.left, geometry.right}) {
            ShatterShardTiming& shard = _shards[static_cast<std::size_t>(index)];
            shard.crackedAt = std::min(shard.crackedAt, timing.start);
            shard.freedAt = std::max(shard.freedAt, timing.end);
        }
    }

    // A shard without boundary cracks (including a one-shard pattern) is free from time zero.
    for (ShatterShardTiming& shard : _shards)
        if (shard.crackedAt == std::numeric_limits<float>::max()) shard.crackedAt = 0.0f;
}
