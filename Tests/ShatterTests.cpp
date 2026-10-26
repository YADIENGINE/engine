#include <catch2/catch_approx.hpp>
#include "Tests/Support/Diagnostics.h"
#include <cmath>
#include <cstddef>
#include <vector>
#include "Fx/ShatterPattern.h"

namespace {
constexpr Rect Bounds{10, 20, 300, 200};

// Dışbükey çokgenin içinde mi (kenarlar dahil, iki sarım yönü de).
bool Contains(const std::vector<Float2>& outline, Float2 point) {
    bool negative = false;
    bool positive = false;
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const Float2 a = outline[i];
        const Float2 b = outline[(i + 1) % outline.size()];
        const float cross = (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
        negative = negative || cross < -1e-3f;
        positive = positive || cross > 1e-3f;
    }
    return !(negative && positive);
}

std::vector<Float2> ScatteredSeeds() {
    std::vector<Float2> seeds;
    for (int i = 0; i < 40; ++i) {
        const float u = std::fmod(static_cast<float>(i) * 0.618034f, 1.0f);
        const float v = std::fmod(static_cast<float>(i) * 0.414214f + 0.13f, 1.0f);
        seeds.push_back({static_cast<float>(Bounds.x) + u * static_cast<float>(Bounds.width),
                         static_cast<float>(Bounds.y) + v * static_cast<float>(Bounds.height)});
    }
    return seeds;
}
} // namespace

TEST_CASE("Engine.Shatter.ShardsTileTheBounds", "[engine][fx]") {
    const std::vector<Float2> seeds = ScatteredSeeds();
    const ShatterPattern pattern(Bounds, seeds);
    REQUIRE(pattern.Shards().size() == seeds.size());
    float area = 0.0f;
    for (std::size_t i = 0; i < seeds.size(); ++i) {
        const ShatterShard& shard = pattern.Shards()[i];
        REQUIRE(shard.outline.size() >= 3);
        CHECK(Contains(shard.outline, seeds[i]));
        CHECK(Contains(shard.outline, shard.center));
        CHECK(pattern.ShardAt(seeds[i]) == static_cast<int>(i));
        area += shard.area;
    }
    CHECK(area == Catch::Approx(static_cast<float>(Bounds.width * Bounds.height)).epsilon(1e-3));
}

TEST_CASE("Engine.Shatter.CracksSeparateNeighboursAndSkipBorder", "[engine][fx]") {
    const std::vector<Float2> seeds{{85.0f, 120.0f}, {235.0f, 120.0f}};
    const ShatterPattern pattern(Bounds, seeds);
    REQUIRE(pattern.Cracks().size() == 1);
    const ShatterCrack& crack = pattern.Cracks().front();
    CHECK(crack.left == 0);
    CHECK(crack.right == 1);
    CHECK(crack.from.x == Catch::Approx(160.0f));
    CHECK(crack.to.x == Catch::Approx(160.0f));
    CHECK(crack.length == Catch::Approx(static_cast<float>(Bounds.height)));
}

TEST_CASE("Engine.Shatter.IgnoresOutsideAndRepeatedSeeds", "[engine][fx]") {
    const std::vector<Float2> seeds{{85.0f, 120.0f}, {-50.0f, 0.0f}, {85.0f, 120.0f}};
    const ShatterPattern pattern(Bounds, seeds);
    CHECK(pattern.Shards()[0].area == Catch::Approx(static_cast<float>(Bounds.width * Bounds.height)));
    CHECK(pattern.Shards()[1].outline.empty());
    CHECK(pattern.Shards()[2].outline.empty());
    CHECK(pattern.Cracks().empty());
    CHECK(pattern.ShardAt({-50.0f, 0.0f}) == 0);
}

TEST_CASE("Engine.Shatter.SpreadOpensCracksFromTheNearEnd", "[engine][fx]") {
    const std::vector<Float2> seeds{{85.0f, 120.0f}, {235.0f, 120.0f}};
    ShatterPattern pattern(Bounds, seeds);
    const std::vector<CrackSource> sources{{.point = {160.0f, 20.0f}, .time = 1.0f}};
    pattern.Spread(sources, 100.0f);
    const ShatterCrack& crack = pattern.Cracks().front();
    CHECK(crack.from.y == Catch::Approx(20.0f));
    CHECK(crack.to.y == Catch::Approx(220.0f));
    CHECK(crack.start == Catch::Approx(1.0f));
    CHECK(crack.end == Catch::Approx(3.0f));
    for (const ShatterShard& shard : pattern.Shards()) {
        CHECK(shard.crackedAt == Catch::Approx(1.0f));
        CHECK(shard.freedAt == Catch::Approx(3.0f));
    }
}

TEST_CASE("Engine.Shatter.SpreadReachesNearShardsFirst", "[engine][fx]") {
    ShatterPattern pattern(Bounds, ScatteredSeeds());
    const Float2 origin{20.0f, 30.0f};
    pattern.Spread(std::vector<CrackSource>{{.point = origin}}, 200.0f);
    const int near = pattern.ShardAt(origin);
    const int far = pattern.ShardAt({300.0f, 210.0f});
    REQUIRE(near >= 0);
    REQUIRE(far >= 0);
    const auto& shards = pattern.Shards();
    CHECK(shards[static_cast<std::size_t>(near)].crackedAt < shards[static_cast<std::size_t>(far)].crackedAt);
    for (const ShatterShard& shard : shards)
        CHECK(shard.crackedAt <= shard.freedAt);
    for (const ShatterCrack& crack : pattern.Cracks())
        CHECK(crack.start <= crack.end);
}
