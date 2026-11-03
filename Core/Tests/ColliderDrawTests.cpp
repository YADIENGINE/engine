#include <catch2/catch_test_macros.hpp>
#include <array>
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Debug/ColliderDraw.h"
#include "Tests/PaintHarness.h"

namespace {
enum class Layer { Enemy, Wall };

CollisionWorld WorldWithEveryKind() {
    constexpr std::array<Float2, 3> corners{Float2{-6, -6}, Float2{6, -6}, Float2{0, 6}};
    CollisionWorld world;
    (void)world.Add(Shape::Circle(5.0f), {{50.0f, 50.0f}}, MaskOf(Layer::Enemy));
    (void)world.Add(Shape::Box(5.0f, 8.0f), {{100.0f, 50.0f}, 0.5f}, MaskOf(Layer::Enemy));
    (void)world.Add(Shape::Capsule({-5, 0}, {5, 0}, 3.0f), {{150.0f, 50.0f}, 1.0f}, MaskOf(Layer::Enemy));
    (void)world.Add(Shape::Segment({-5, 0}, {5, 0}), {{200.0f, 50.0f}}, MaskOf(Layer::Wall));
    (void)world.Add(Shape::Polygon(corners), {{250.0f, 50.0f}}, MaskOf(Layer::Enemy));
    return world;
}
} // namespace

TEST_CASE("Engine.Physics.Draw.OutlinesEveryKindOfShape", "[engine][physics]") {
    const auto world = WorldWithEveryKind();
    PaintHarness h;
    h.Begin();
    DrawColliders(h.paint, world);
    h.End();
    REQUIRE_FALSE(h.Draws().empty());
    CHECK_FALSE(h.Drawn().empty());
}
TEST_CASE("Engine.Physics.Draw.ShowsOnlyTheAskedLayers", "[engine][physics]") {
    const auto world = WorldWithEveryKind();
    const auto vertices = [&](CollisionMask layers) {
        PaintHarness h;
        h.Begin();
        DrawColliders(h.paint, world, {.layers = layers});
        h.End();
        std::size_t count = 0;
        for (std::size_t i = 0; i < h.Draws().size(); ++i)
            count += h.Drawn(i).size();
        return count;
    };
    const auto all = vertices(CollisionMask::All());
    const auto walls = vertices(MaskOf(Layer::Wall));
    CHECK(vertices(CollisionMask{}) == 0);
    CHECK(walls > 0);
    CHECK(walls < all);
}
