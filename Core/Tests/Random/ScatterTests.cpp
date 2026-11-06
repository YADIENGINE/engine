#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>
#include "Utilities/Random/Scatter.h"

namespace {
float NearestOther(const std::vector<Float2>& points, std::size_t index) {
    float nearest = std::numeric_limits<float>::max();
    for (std::size_t j = 0; j < points.size(); ++j)
        if (j != index) nearest = std::min(nearest, Distance(points[index], points[j]));
    return nearest;
}
} // namespace

TEST_CASE("Engine.Scatter.PointsKeepTheirSpacingInsideTheArea", "[engine][random]") {
    const FloatRect area{10.0f, -20.0f, 300.0f, 200.0f};
    Rng rng(4);
    const auto points = ScatterPoints(area, 12.0f, rng);
    REQUIRE(points.size() > 150);
    for (std::size_t i = 0; i < points.size(); ++i) {
        REQUIRE(area.Contains(points[i]));
        REQUIRE(NearestOther(points, i) >= 12.0f);
    }
}
TEST_CASE("Engine.Scatter.LeavesNoGapWhereAPointWouldFit", "[engine][random]") {
    const FloatRect area{0.0f, 0.0f, 200.0f, 200.0f};
    Rng rng(8);
    const auto points = ScatterPoints(area, 10.0f, rng);
    // Every spot of the area is within twice the spacing of a point (Bridson's guarantee, up to unlucky attempts).
    int gaps = 0;
    for (float y = 0.5f; y < area.height; y += 4.0f)
        for (float x = 0.5f; x < area.width; x += 4.0f) {
            float nearest = std::numeric_limits<float>::max();
            for (const Float2 point : points)
                nearest = std::min(nearest, Distance(point, {x, y}));
            if (nearest >= 20.0f) ++gaps;
        }
    CHECK(gaps == 0);
}
TEST_CASE("Engine.Scatter.TheSameGeneratorGivesTheSamePoints", "[engine][random]") {
    const FloatRect area{0.0f, 0.0f, 640.0f, 360.0f};
    Rng first(21), second(21), other(22);
    const auto points = ScatterPoints(area, 20.0f, first);
    CHECK(points == ScatterPoints(area, 20.0f, second));
    CHECK(first == second); // And leaves both generators alike
    CHECK_FALSE(points == ScatterPoints(area, 20.0f, other));
}
TEST_CASE("Engine.Scatter.BadInputGivesNoPoints", "[engine][random]") {
    Rng rng(1);
    CHECK(ScatterPoints({0.0f, 0.0f, 0.0f, 50.0f}, 5.0f, rng).empty());
    CHECK(ScatterPoints({0.0f, 0.0f, 50.0f, 50.0f}, 0.0f, rng).empty());
    CHECK(ScatterPoints({0.0f, 0.0f, 50.0f, 50.0f}, -3.0f, rng).empty());
    CHECK(ScatterPoints({0.0f, 0.0f, 50.0f, 50.0f}, std::nanf(""), rng).empty());
    CHECK(ScatterPoints({0.0f, 0.0f, 1.0e6f, 1.0e6f}, 0.01f, rng).empty()); // Far too many cells
    const auto one = ScatterPoints({0.0f, 0.0f, 4.0f, 4.0f}, 100.0f, rng);  // Room for only one
    CHECK(one.size() == 1);
}
