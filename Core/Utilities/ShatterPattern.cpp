#include "Fx/ShatterPattern.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {
constexpr float MinCrackLength = 0.5f;
constexpr float SameSeedDistance = 1e-3f;
constexpr int Border = -1;

// Kırığın köşesi; 'edge' bu köşeden sonrakine giden kenarın öbür yanındaki kırık (dikdörtgen kenarıysa Border).
struct Corner {
    Float2 point;
    int edge = Border;
};

// Çokgenin 'seed'e 'other'dan yakın olan yarısı (Sutherland–Hodgman); kesimden doğan kenar 'neighbour'ındır.
std::vector<Corner> Clip(const std::vector<Corner>& polygon, Float2 seed, Float2 other, int neighbour) {
    const Float2 normal{other.x - seed.x, other.y - seed.y};
    const Float2 middle = Mix(seed, other, 0.5f);
    const auto side = [&](Float2 p) { return (p.x - middle.x) * normal.x + (p.y - middle.y) * normal.y; };
    std::vector<Corner> clipped;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Corner& current = polygon[i];
        const Corner& next = polygon[(i + 1) % polygon.size()];
        const float a = side(current.point);
        const float b = side(next.point);
        if (a <= 0.0f) clipped.push_back(current);
        if ((a <= 0.0f) != (b <= 0.0f)) {
            // Çıkarken yeni kenar kesim çizgisi boyuncadır; girerken eski kenarın kalanıdır.
            const Float2 cut = Mix(current.point, next.point, a / (a - b));
            clipped.push_back({cut, a <= 0.0f ? neighbour : current.edge});
        }
    }
    return clipped;
}

void Measure(ShatterShard& shard) {
    float area = 0.0f;
    Float2 sum;
    const std::size_t count = shard.outline.size();
    for (std::size_t i = 0; i < count; ++i) {
        const Float2 a = shard.outline[i];
        const Float2 b = shard.outline[(i + 1) % count];
        const float cross = a.x * b.y - b.x * a.y;
        area += cross;
        sum.x += (a.x + b.x) * cross;
        sum.y += (a.y + b.y) * cross;
    }
    shard.area = std::abs(area) / 2.0f;
    if (std::abs(area) > std::numeric_limits<float>::epsilon()) {
        shard.center = {sum.x / (3.0f * area), sum.y / (3.0f * area)};
    } else {
        shard.center = shard.seed;
    }
}
} // namespace

ShatterPattern::ShatterPattern(const Rect& bounds, std::span<const Float2> seeds) {
    const auto left = static_cast<float>(bounds.x);
    const auto top = static_cast<float>(bounds.y);
    const auto right = static_cast<float>(bounds.Right());
    const auto bottom = static_cast<float>(bounds.Bottom());
    // Dikdörtgenin dışındaki ve öncekini tekrarlayan tohumlar kırık doğurmaz, başkasını da kesmez.
    std::vector<bool> used(seeds.size(), false);
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        const Float2 seed = seeds[i];
        const bool inside = seed.x >= left && seed.x <= right && seed.y >= top && seed.y <= bottom;
        const bool repeated =
            std::ranges::any_of(seeds.first(i), [&](Float2 other) { return Distance(seed, other) < SameSeedDistance; });
        used[i] = inside && !repeated;
    }
    _shards.resize(seeds.size());
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        ShatterShard& shard = _shards[i];
        shard.seed = seeds[i];
        if (!used[i]) continue;
        std::vector<Corner> polygon{{{left, top}}, {{right, top}}, {{right, bottom}}, {{left, bottom}}};
        for (std::size_t j = 0; j < seeds.size() && !polygon.empty(); ++j)
            if (j != i && used[j]) polygon = Clip(polygon, seeds[i], seeds[j], static_cast<int>(j));
        for (std::size_t k = 0; k < polygon.size(); ++k) {
            shard.outline.push_back(polygon[k].point);
            // Her çatlak bir kez: küçük indeksli kırığın tarafından.
            const int neighbour = polygon[k].edge;
            if (neighbour <= static_cast<int>(i)) continue;
            const Float2 from = polygon[k].point;
            const Float2 to = polygon[(k + 1) % polygon.size()].point;
            const float length = Distance(from, to);
            if (length >= MinCrackLength)
                _cracks.push_back(
                    {.from = from, .to = to, .length = length, .left = static_cast<int>(i), .right = neighbour});
        }
        Measure(shard);
    }
}

void ShatterPattern::Spread(std::span<const CrackSource> sources, float speed) {
    const float pace = speed > 0.0f ? 1.0f / speed : 0.0f;
    const auto arrival = [&](Float2 point) {
        float best = sources.empty() ? 0.0f : std::numeric_limits<float>::max();
        for (const CrackSource& source : sources)
            best = std::min(best, source.time + Distance(point, source.point) * pace);
        return best;
    };
    for (ShatterShard& shard : _shards) {
        shard.crackedAt = std::numeric_limits<float>::max();
        shard.freedAt = 0.0f;
    }
    for (ShatterCrack& crack : _cracks) {
        const float from = arrival(crack.from);
        const float to = arrival(crack.to);
        if (to < from) std::swap(crack.from, crack.to);
        crack.start = std::min(from, to);
        crack.end = crack.start + crack.length * pace;
        for (const int index : {crack.left, crack.right}) {
            ShatterShard& shard = _shards[static_cast<std::size_t>(index)];
            shard.crackedAt = std::min(shard.crackedAt, crack.start);
            shard.freedAt = std::max(shard.freedAt, crack.end);
        }
    }
    // Çatlaksız kırık (tek kırıklı desen) baştan serbesttir.
    for (ShatterShard& shard : _shards)
        if (shard.crackedAt == std::numeric_limits<float>::max()) shard.crackedAt = 0.0f;
}

int ShatterPattern::ShardAt(Float2 point) const {
    int best = -1;
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < _shards.size(); ++i) {
        if (_shards[i].outline.empty()) continue;
        const float distance = Distance(point, _shards[i].seed);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int>(i);
        }
    }
    return best;
}
