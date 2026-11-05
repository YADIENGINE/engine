#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Graphics/Light/CollisionOccluders.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/Shape.h"
#include "Tests/PaintHarness.h"
#include "Utilities/Geometry/Visibility.h"
#include "Utilities/Math/Scalar.h"

using Catch::Approx;

namespace {
enum class Layer { Wall, Glass };

// Whether 'point' is inside the polygon, by crossings. The polygon may be any simple shape.
bool Contains(std::span<const Float2> polygon, Float2 point) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Float2 a = polygon[i];
        const Float2 b = polygon[j];
        if ((a.y > point.y) == (b.y > point.y)) continue;
        if (point.x < a.x + (point.y - a.y) / (b.y - a.y) * (b.x - a.x)) inside = !inside;
    }
    return inside;
}

bool Crosses(Float2 a, Float2 b, const VisibilityEdge& edge) {
    const Float2 d = b - a;
    const Float2 e = edge.b - edge.a;
    const float facing = Cross(d, e);
    if (facing == 0.0f) return false;
    const float along = Cross(edge.a - a, e) / facing;
    const float on = Cross(edge.a - a, d) / facing;
    return along > 0.0f && along < 1.0f && on > 0.0f && on < 1.0f;
}

// A small generator of its own: the same walls on every run.
struct Numbers {
    std::uint32_t state = 12345;
    float Next(float low, float high) {
        state = state * 1664525u + 1013904223u;
        const float share = static_cast<float>(state >> 8) / static_cast<float>(1u << 24);
        return low + (high - low) * share;
    }
};

std::array<Float2, 4> Square(Float2 low, Float2 high) {
    return {low, Float2{high.x, low.y}, high, Float2{low.x, high.y}};
}

std::vector<VisibilityEdge> Blocking(const LightOccluders& occluders, Float2 from, float radius = 100.0f) {
    std::vector<VisibilityEdge> edges;
    occluders.AppendBlocking(from, radius, edges);
    return edges;
}
} // namespace

TEST_CASE("Engine.Visibility.WithNothingInTheWayCoversTheRadius", "[engine][light]") {
    VisibilityPolygon visibility;
    const auto outline = visibility.From({50.0f, 60.0f}, 100.0f, {});
    REQUIRE(outline.size() >= 4);
    for (const float angle : {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f})
        CHECK(Contains(outline, Float2{50.0f, 60.0f} + Float2::FromAngle(angle, 99.0f)));
    CHECK_FALSE(Contains(outline, {50.0f + 150.0f, 60.0f}));
}

TEST_CASE("Engine.Visibility.AWallCastsAShadowBehindIt", "[engine][light]") {
    VisibilityPolygon visibility;
    const std::array wall{VisibilityEdge{{20.0f, -30.0f}, {20.0f, 30.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, wall);
    CHECK(Contains(outline, {19.0f, 0.0f}));
    CHECK_FALSE(Contains(outline, {21.0f, 0.0f}));
    CHECK_FALSE(Contains(outline, {80.0f, 100.0f})); // On the far side of the wall's end
    CHECK_FALSE(Contains(outline, {80.0f, 119.0f}));
    CHECK(Contains(outline, {0.0f, 99.0f}));         // Beside the wall
    CHECK(Contains(outline, {30.0f, 80.0f}));        // Past the wall's end: the line from the light misses it
    CHECK(Contains(outline, {-90.0f, 0.0f}));        // Behind the light
}

TEST_CASE("Engine.Visibility.WallsBeyondTheRadiusChangeNothing", "[engine][light]") {
    VisibilityPolygon visibility;
    const std::array far{VisibilityEdge{{200.0f, -30.0f}, {200.0f, 30.0f}}};
    const std::size_t alone = visibility.From({0.0f, 0.0f}, 100.0f, {}).size();
    CHECK(visibility.From({0.0f, 0.0f}, 100.0f, far).size() == alone);
}

TEST_CASE("Engine.Visibility.ALongWallIsCutWhereItLeavesTheReach", "[engine][light]") {
    VisibilityPolygon visibility;
    // Both ends far outside: without a cut, nothing marks where the wall meets the outline's box.
    const std::array wall{VisibilityEdge{{20.0f, -5000.0f}, {20.0f, 5000.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, wall);
    for (float y = -95.0f; y <= 95.0f; y += 5.0f) {
        CHECK(Contains(outline, {19.0f, y}));
        CHECK_FALSE(Contains(outline, {21.0f, y}));
    }
    CHECK(Contains(outline, {-95.0f, 95.0f}));
}

TEST_CASE("Engine.Visibility.NoLightPassesBetweenEdgesSharingACorner", "[engine][light]") {
    VisibilityPolygon visibility;
    const std::array corner{VisibilityEdge{{30.0f, -30.0f}, {30.0f, 30.0f}},
                            VisibilityEdge{{30.0f, 30.0f}, {-30.0f, 30.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, corner);
    CHECK_FALSE(Contains(outline, {31.0f, 31.0f}));
    CHECK_FALSE(Contains(outline, {50.0f, 50.0f}));
    CHECK_FALSE(Contains(outline, {60.0f, 61.0f}));
    CHECK_FALSE(Contains(outline, {61.0f, 60.0f}));

    // The ray runs exactly through the point where two edges touch.
    const std::array touching{VisibilityEdge{{0.0f, 40.0f}, {40.0f, 40.0f}},
                              VisibilityEdge{{40.0f, 40.0f}, {40.0f, 0.0f}},
                              VisibilityEdge{{40.0f, 40.0f}, {80.0f, 40.0f}},
                              VisibilityEdge{{40.0f, 40.0f}, {40.0f, 80.0f}}};
    const auto around = visibility.From({0.0f, 0.0f}, 100.0f, touching);
    CHECK_FALSE(Contains(around, {60.0f, 60.0f}));
}

TEST_CASE("Engine.Visibility.TwoWallsThatCrossLeaveNoGapWhereTheyMeet", "[engine][light]") {
    VisibilityPolygon visibility;
    // An X ahead of the point: the nearest wall changes at the crossing, where neither has a corner.
    const std::array walls{VisibilityEdge{{30.0f, -40.0f}, {70.0f, 40.0f}},
                           VisibilityEdge{{70.0f, -40.0f}, {30.0f, 40.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, walls);
    CHECK(Contains(outline, {48.0f, 0.0f})); // Up to the crossing, at 50
    CHECK_FALSE(Contains(outline, {53.0f, 0.0f}));
    CHECK(Contains(outline, {38.0f, 10.0f})); // In front of a wall beside it
    CHECK_FALSE(Contains(outline, {50.0f, 10.0f}));
}

TEST_CASE("Engine.Visibility.AWallThroughThePointBlocksNothing", "[engine][light]") {
    VisibilityPolygon visibility;
    const std::array through{VisibilityEdge{{-50.0f, 0.0f}, {50.0f, 0.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, through);
    CHECK(Contains(outline, {0.0f, 50.0f}));
    CHECK(Contains(outline, {0.0f, -50.0f}));
}

TEST_CASE("Engine.Visibility.EdgesInLineWithThePointAreHandled", "[engine][light]") {
    VisibilityPolygon visibility;
    // One wall seen exactly end-on, and a wall beyond it that it does not hide.
    const std::array walls{VisibilityEdge{{20.0f, 0.0f}, {60.0f, 0.0f}},
                           VisibilityEdge{{70.0f, -20.0f}, {70.0f, 20.0f}}};
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, walls);
    CHECK(Contains(outline, {65.0f, 5.0f}));
    CHECK(Contains(outline, {65.0f, -5.0f}));
    CHECK_FALSE(Contains(outline, {75.0f, 5.0f}));
    // A degenerate edge changes nothing, and neither does one not finite.
    const std::array odd{VisibilityEdge{{30.0f, 30.0f}, {30.0f, 30.0f}},
                         VisibilityEdge{{std::numeric_limits<float>::quiet_NaN(), 0.0f}, {1.0f, 1.0f}}};
    CHECK(Contains(visibility.From({0.0f, 0.0f}, 100.0f, odd), {60.0f, 60.0f}));
}

TEST_CASE("Engine.Visibility.InvalidInputGivesNothing", "[engine][light]") {
    VisibilityPolygon visibility;
    CHECK(visibility.From({0.0f, 0.0f}, 0.0f, {}).empty());
    CHECK(visibility.From({0.0f, 0.0f}, -1.0f, {}).empty());
    CHECK(visibility.From({std::numeric_limits<float>::quiet_NaN(), 0.0f}, 10.0f, {}).empty());
    CHECK(visibility.From({0.0f, 0.0f}, std::numeric_limits<float>::infinity(), {}).empty());
}

// The strongest check: the polygon against what it stands for, for many walls and points, away from the exact edges
// where either answer is right.
TEST_CASE("Engine.Visibility.AgreesWithASightLineToEveryPoint", "[engine][light]") {
    constexpr float Radius = 100.0f;
    constexpr float Clearance = 0.75f; // Points this close to an edge or a shadow's border are not asked about
    Numbers numbers;
    VisibilityPolygon visibility;
    int compared = 0;
    for (int round = 0; round < 40; ++round) {
        std::vector<VisibilityEdge> walls;
        const int count = 1 + round % 12;
        for (int i = 0; i < count; ++i) {
            const Float2 at{numbers.Next(-90.0f, 90.0f), numbers.Next(-90.0f, 90.0f)};
            walls.push_back({at, at + Float2::FromAngle(numbers.Next(0.0f, 6.28f), numbers.Next(5.0f, 60.0f))});
        }
        const Float2 from{numbers.Next(-30.0f, 30.0f), numbers.Next(-30.0f, 30.0f)};
        const auto outline = visibility.From(from, Radius, walls);
        const std::vector<Float2> polygon(outline.begin(), outline.end());
        for (int i = 0; i < 300; ++i) {
            const float angle = numbers.Next(0.0f, 6.28f);
            const Float2 point = from + Float2::FromAngle(angle, numbers.Next(1.0f, Radius * 0.98f));
            const auto clear = [&](Float2 target) {
                for (const VisibilityEdge& wall : walls)
                    if (Crosses(from, target, wall)) return false;
                return true;
            };
            // Only points whose neighbours agree: the answer right at a border depends on rounding.
            const Float2 side = Perpendicular(Normalized(point - from)) * Clearance;
            const Float2 along = Normalized(point - from) * Clearance;
            const bool seen = clear(point);
            if (clear(point + side) != seen || clear(point - side) != seen || clear(point + along) != seen ||
                clear(point - along) != seen)
                continue;
            CHECK(Contains(polygon, point) == seen);
            ++compared;
        }
    }
    CHECK(compared > 5000);
}

TEST_CASE("Engine.LightOccluders.AnOutlineBlocksOnlyItsFarSideWhenSelfLit", "[engine][light]") {
    LightOccluders occluders;
    const auto box = Square({20.0f, -10.0f}, {40.0f, 10.0f});
    occluders.Outline(box);
    REQUIRE(occluders.Edges().size() == 4);
    const auto blocking = Blocking(occluders, {0.0f, 0.0f});
    REQUIRE(blocking.size() == 3); // Seen from the left, only the left side faces the light
    for (const VisibilityEdge& edge : blocking)
        CHECK(std::max(edge.a.x, edge.b.x) == Approx(40.0f));

    VisibilityPolygon visibility;
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, blocking);
    CHECK(Contains(outline, {30.0f, 0.0f}));       // Its inside is lit
    CHECK_FALSE(Contains(outline, {45.0f, 0.0f})); // The shadow starts behind it
}

TEST_CASE("Engine.LightOccluders.DirectionAndMirroringDoNotChangeWhatBlocks", "[engine][light]") {
    const std::vector<Float2> forward{Float2{20.0f, -10.0f}, {40.0f, -10.0f}, {40.0f, 10.0f}, {20.0f, 10.0f}};
    const std::vector<Float2> backward{Float2{20.0f, 10.0f}, {40.0f, 10.0f}, {40.0f, -10.0f}, {20.0f, -10.0f}};
    const auto farSide = [](const LightOccluders& occluders) {
        const auto blocking = Blocking(occluders, {0.0f, 0.0f});
        REQUIRE(blocking.size() == 3);
        float lowest = 1e9f;
        for (const VisibilityEdge& edge : blocking)
            lowest = std::min({lowest, std::max(edge.a.x, edge.b.x)});
        return lowest;
    };
    LightOccluders one;
    one.Outline(forward);
    LightOccluders two;
    two.Outline(backward);
    LightOccluders mirrored;
    const std::vector<Float2> left{Float2{-20.0f, -10.0f}, {-40.0f, -10.0f}, {-40.0f, 10.0f}, {-20.0f, 10.0f}};
    mirrored.Outline(left, Affine2{.a = -1.0f}); // x is mirrored: the same place, the other direction
    CHECK(farSide(one) == Approx(40.0f));
    CHECK(farSide(two) == Approx(40.0f));
    CHECK(farSide(mirrored) == Approx(40.0f));
}

TEST_CASE("Engine.LightOccluders.ASmallShapeFarFromTheOriginKeepsItsDirection", "[engine][light]") {
    constexpr Float2 Far{50000.0f, 50000.0f};
    const std::vector<Float2> forward{{0.0f, 0.0f}, {4.0f, 0.0f}, {4.0f, 4.0f}, {0.0f, 4.0f}};
    const std::vector<Float2> backward{{0.0f, 4.0f}, {4.0f, 4.0f}, {4.0f, 0.0f}, {0.0f, 0.0f}};
    for (const auto* points : {&forward, &backward}) {
        LightOccluders occluders;
        occluders.Outline(*points, Affine2::Translate(Far));
        const auto blocking = Blocking(occluders, Far + Float2{-20.0f, 2.0f}); // From the left
        CHECK(blocking.size() == 3);
        for (const VisibilityEdge& edge : blocking)
            CHECK(std::max(edge.a.x, edge.b.x) == Approx(Far.x + 4.0f));
    }
}

TEST_CASE("Engine.LightOccluders.WithoutSelfLightTheNearSideBlocksToo", "[engine][light]") {
    LightOccluders occluders;
    occluders.Outline(Square({20.0f, -10.0f}, {40.0f, 10.0f}), {}, false);
    CHECK(Blocking(occluders, {0.0f, 0.0f}).size() == 4);
    VisibilityPolygon visibility;
    const auto outline = visibility.From({0.0f, 0.0f}, 100.0f, Blocking(occluders, {0.0f, 0.0f}));
    CHECK_FALSE(Contains(outline, {30.0f, 0.0f}));
    CHECK(Contains(outline, {19.0f, 0.0f}));
}

TEST_CASE("Engine.LightOccluders.ALightInsideAnOutlineIsNotStoppedByIt", "[engine][light]") {
    for (const bool selfLit : {true, false}) {
        LightOccluders occluders;
        occluders.Outline(Square({-10.0f, -10.0f}, {10.0f, 10.0f}), {}, selfLit);
        CHECK(Blocking(occluders, {0.0f, 0.0f}).empty());
        CHECK_FALSE(Blocking(occluders, {15.0f, 0.0f}).empty());
    }
    // In the notch of a shape that is not convex the light is outside it.
    LightOccluders notched;
    const std::vector<Float2> shape{Float2{0.0f, 0.0f}, {30.0f, 0.0f}, {30.0f, 30.0f}, {20.0f, 30.0f},
                           {20.0f, 10.0f},     {10.0f, 10.0f}, {10.0f, 30.0f}, {0.0f, 30.0f}};
    notched.Outline(shape);
    CHECK(Blocking(notched, {15.0f, 5.0f}).empty());
    CHECK_FALSE(Blocking(notched, {15.0f, 20.0f}).empty());
}

TEST_CASE("Engine.LightOccluders.ALineBlocksFromBothSidesAndClosesNothing", "[engine][light]") {
    LightOccluders occluders;
    const std::vector<Float2> bend{Float2{20.0f, -10.0f}, {20.0f, 10.0f}, {40.0f, 10.0f}};
    occluders.Line(bend);
    CHECK(occluders.Edges().size() == 2);
    CHECK(Blocking(occluders, {0.0f, 0.0f}).size() == 2);
    CHECK(Blocking(occluders, {30.0f, 0.0f}).size() == 2);
}

TEST_CASE("Engine.LightOccluders.PartsOutOfReachAreSkipped", "[engine][light]") {
    LightOccluders occluders;
    occluders.Outline(Square({500.0f, 500.0f}, {520.0f, 520.0f}));
    CHECK(Blocking(occluders, {0.0f, 0.0f}, 100.0f).empty());
    CHECK_FALSE(Blocking(occluders, {0.0f, 0.0f}, 2000.0f).empty());
}

TEST_CASE("Engine.LightOccluders.RefusesInvalidPointsAndClears", "[engine][light]") {
    LightOccluders occluders;
    const std::vector<Float2> two{Float2{0.0f, 0.0f}, {1.0f, 1.0f}};
    const std::vector<Float2> one{Float2{0.0f, 0.0f}};
    const std::vector<Float2> bad{Float2{0.0f, 0.0f}, {std::numeric_limits<float>::infinity(), 1.0f}, {1.0f, 0.0f}};
    CHECK_THROWS_AS(occluders.Outline(two), std::invalid_argument);
    CHECK_THROWS_AS(occluders.Line(one), std::invalid_argument);
    CHECK_THROWS_AS(occluders.Outline(bad), std::invalid_argument);
    CHECK(occluders.Empty());
    occluders.Line(two);
    CHECK_FALSE(occluders.Empty());
    occluders.Clear();
    CHECK(occluders.Empty());
    CHECK(Blocking(occluders, {0.0f, 0.0f}).empty());
}

TEST_CASE("Engine.LightOccluders.PlacedByTheirTransform", "[engine][light]") {
    LightOccluders occluders;
    occluders.Outline(Square({-10.0f, -10.0f}, {10.0f, 10.0f}),
                      Affine2::Translate({100.0f, 0.0f}) * Affine2::Rotate(Pi / 2.0f));
    float low = 1e9f;
    float high = -1e9f;
    for (const VisibilityEdge& edge : occluders.Edges()) {
        low = std::min(low, edge.a.x);
        high = std::max(high, edge.a.x);
    }
    CHECK(low == Approx(90.0f));
    CHECK(high == Approx(110.0f));
}

TEST_CASE("Engine.CollisionOccluders.BringOnlyTheAskedLayers", "[engine][light]") {
    CollisionWorld world;
    (void)world.Add(Shape::Box(10.0f, 10.0f), {{100.0f, 0.0f}}, MaskOf(Layer::Wall));
    (void)world.Add(Shape::Box(10.0f, 10.0f), {{-100.0f, 0.0f}}, MaskOf(Layer::Glass));
    LightOccluders occluders;
    AddOccluders(occluders, world, MaskOf(Layer::Wall));
    REQUIRE(occluders.Edges().size() == 4);
    for (const VisibilityEdge& edge : occluders.Edges())
        CHECK(edge.a.x > 0.0f);
}

TEST_CASE("Engine.CollisionOccluders.FollowTheShapeAndItsPose", "[engine][light]") {
    CollisionWorld world;
    const std::vector<Float2> points{Float2{0.0f, 0.0f}, {20.0f, 0.0f}, {0.0f, 20.0f}};
    (void)world.Add(Shape::Polygon(points), {{100.0f, 0.0f}, Pi}, MaskOf(Layer::Wall));
    (void)world.Add(Shape::Segment({0.0f, 0.0f}, {50.0f, 0.0f}), {{0.0f, 200.0f}}, MaskOf(Layer::Wall));
    LightOccluders occluders;
    AddOccluders(occluders, world, MaskOf(Layer::Wall));
    REQUIRE(occluders.Edges().size() == 4); // The triangle's three sides, and a line
    // A half turn around (100, 0): the triangle now reaches left of its position.
    float low = 1e9f;
    for (const VisibilityEdge& edge : occluders.Edges().subspan(0, 3))
        low = std::min({low, edge.a.x, edge.b.x});
    CHECK(low == Approx(80.0f));
    CHECK(occluders.Edges()[3].b.x == Approx(50.0f));
    CHECK(occluders.Edges()[3].a.y == Approx(200.0f));
}

TEST_CASE("Engine.CollisionOccluders.TurnCurvesIntoShadowsOfTheirSize", "[engine][light]") {
    CollisionWorld world;
    (void)world.Add(Shape::Circle(10.0f, {5.0f, 0.0f}), {{100.0f, 0.0f}}, MaskOf(Layer::Wall));
    (void)world.Add(Shape::Capsule({0.0f, -20.0f}, {0.0f, 20.0f}, 5.0f), {{200.0f, 0.0f}}, MaskOf(Layer::Wall));
    LightOccluders occluders;
    AddOccluders(occluders, world, MaskOf(Layer::Wall));
    float circleLow = 1e9f;
    float circleHigh = -1e9f;
    float capsuleLowY = 1e9f;
    float capsuleHighY = -1e9f;
    float capsuleLowX = 1e9f;
    float capsuleHighX = -1e9f;
    for (const VisibilityEdge& edge : occluders.Edges()) {
        if (edge.a.x < 150.0f) {
            circleLow = std::min(circleLow, edge.a.x);
            circleHigh = std::max(circleHigh, edge.a.x);
        } else {
            capsuleLowX = std::min(capsuleLowX, edge.a.x);
            capsuleHighX = std::max(capsuleHighX, edge.a.x);
            capsuleLowY = std::min(capsuleLowY, edge.a.y);
            capsuleHighY = std::max(capsuleHighY, edge.a.y);
        }
    }
    CHECK(circleLow == Approx(95.0f));
    CHECK(circleHigh == Approx(115.0f));
    CHECK(capsuleLowX == Approx(195.0f));
    CHECK(capsuleHighX == Approx(205.0f));
    CHECK(capsuleLowY == Approx(-25.0f));
    CHECK(capsuleHighY == Approx(25.0f));

    // The outlines are simple: a light beside either finds only their far side blocking, and none from inside.
    CHECK(Blocking(occluders, {105.0f, 0.0f}, 20.0f).empty());
    CHECK(Blocking(occluders, {200.0f, 22.0f}, 20.0f).empty());
    CHECK_FALSE(Blocking(occluders, {150.0f, 0.0f}).empty());
}

TEST_CASE("Engine.CollisionOccluders.ReadTheWorldWithoutChangingIt", "[engine][light]") {
    CollisionWorld world;
    const ColliderId id = world.Add(Shape::Box(10.0f, 10.0f), {{100.0f, 0.0f}}, MaskOf(Layer::Wall));
    LightOccluders occluders;
    AddOccluders(occluders, world, MaskOf(Layer::Wall));
    CHECK(world.Count() == 1);
    CHECK(world.PoseOf(id).position == Float2{100.0f, 0.0f});
}

TEST_CASE("Engine.LightLayer.DrawsAFanWhenSomethingStopsTheLight", "[engine][paint][light]") {
    PaintHarness h({400.0f, 200.0f}, 1.0f);
    LightLayer layer({.resolution = 1.0f});
    LightOccluders walls;
    const std::vector<Float2> wall{Float2{120.0f, 20.0f}, {120.0f, 180.0f}};
    walls.Line(wall);
    const std::array<const LightOccluders*, 2> sets{&walls, nullptr};
    const std::array lights{LightSource{.at = {100.0f, 100.0f}, .radius = 60.0f},
                            LightSource{.at = {100.0f, 100.0f}, .radius = 60.0f, .shadows = false},
                            LightSource{.at = {300.0f, 100.0f}, .radius = 60.0f}};
    h.Begin();
    layer.Draw(h.paint, {.ambient = {20, 20, 20, 255}, .lights = lights, .occluders = sets});
    h.End();
    const auto drawn = h.Drawn(0);
    // Every light is one batch: a fan, a quad and a quad, in the order they were given.
    REQUIRE(drawn.size() > 12);
    REQUIRE(drawn.size() % 3 == 0);
    for (std::size_t i = 0; i < drawn.size() - 12; i += 3) {
        CHECK(drawn[i].kind == PaintKind::Light);
        CHECK(drawn[i].u == Approx(0.0f)); // Every triangle of the fan starts at the light
        CHECK(drawn[i].v == Approx(0.0f));
    }
    for (std::size_t i = 0; i < drawn.size() - 12; ++i)
        CHECK(drawn[i].x <= 120.5f); // The fan ends at the wall
    CHECK(drawn[drawn.size() - 12].x - drawn[drawn.size() - 12].u == Approx(100.0f));
    CHECK(drawn.back().x - drawn.back().u == Approx(300.0f));
}

TEST_CASE("Engine.LightGuides.ShowWhatStopsLight", "[engine][paint][light]") {
    PaintHarness h({400.0f, 200.0f}, 1.0f);
    LightOccluders walls;
    const std::vector<Float2> wall{Float2{120.0f, 20.0f}, {120.0f, 180.0f}};
    walls.Line(wall);
    const std::array<const LightOccluders*, 2> sets{&walls, nullptr};
    h.Begin();
    DrawLightGuides(h.paint, {.occluders = sets}, {}, {.occluder = {1, 2, 3, 255}});
    h.End();
    const auto drawn = h.Drawn();
    REQUIRE_FALSE(drawn.empty());
    CHECK(drawn[0].color == PaintColor{1, 2, 3, 255});
}
