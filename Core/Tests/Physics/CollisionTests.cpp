#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include "Physics/Collision/Collision.h"
#include "Utilities/MathUtils.h"

using Catch::Approx;

namespace {
constexpr float CastSlack = 0.05f; // A cast stops a hair before the surface
} // namespace

TEST_CASE("Engine.Physics.Overlap.TouchingCounts", "[engine][physics]") {
    const auto circle = Shape::Circle(5.0f);
    CHECK(ShapesOverlap(circle, {}, circle, {{10.0f, 0.0f}}));
    CHECK_FALSE(ShapesOverlap(circle, {}, circle, {{10.5f, 0.0f}}));
    CHECK(ShapeDistance(circle, {}, circle, {{13.0f, 0.0f}}) == Approx(3.0f));
    CHECK(ShapeDistance(circle, {}, circle, {{2.0f, 0.0f}}) == 0.0f);
}
TEST_CASE("Engine.Physics.Overlap.EveryPairOfKindsAnswers", "[engine][physics]") {
    constexpr std::array<Float2, 3> corners{Float2{-6, -6}, Float2{6, -6}, Float2{0, 6}};
    const std::array shapes{Shape::Circle(5.0f), Shape::Box(5.0f, 5.0f), Shape::Capsule({-5, 0}, {5, 0}, 2.0f),
                            Shape::Segment({-5, 0}, {5, 0}), Shape::Polygon(corners)};
    for (const auto& a : shapes) {
        for (const auto& b : shapes) {
            CHECK(ShapesOverlap(a, {}, b, {{1.0f, 0.5f}, 0.7f})); // Turned, so that even two lines cross
            CHECK_FALSE(ShapesOverlap(a, {}, b, {{100.0f, 0.0f}}));
            CHECK(ShapeDistance(a, {}, b, {{100.0f, 0.0f}}) > 70.0f);
        }
    }
}
TEST_CASE("Engine.Physics.Pose.TurnsClockwiseOnScreen", "[engine][physics]") {
    const auto bar = Shape::Box(20.0f, 2.0f);
    CHECK(ShapeContains(bar, {}, {15.0f, 0.0f}));
    CHECK_FALSE(ShapeContains(bar, {}, {0.0f, 15.0f}));
    const Pose turned{{}, Pi / 2.0f};
    CHECK(ShapeContains(bar, turned, {0.0f, 15.0f}));
    CHECK_FALSE(ShapeContains(bar, turned, {15.0f, 0.0f}));

    const auto offset = Shape::Circle(1.0f, {10.0f, 0.0f}); // y grows downwards, so clockwise takes +x to +y
    CHECK(ShapeContains(offset, turned, {0.0f, 10.0f}));
    CHECK(ShapeContains(offset, {{100.0f, 0.0f}, Pi / 2.0f}, {100.0f, 10.0f}));
}
TEST_CASE("Engine.Physics.Pose.AnAngleMayGrowWithoutLimit", "[engine][physics]") {
    const auto bar = Shape::Box(20.0f, 2.0f);
    for (const float turns : {1.0f, 2.0f, 5.0f, -3.0f}) {
        const Pose pose{{}, Pi / 2.0f + turns * 2.0f * Pi};
        CHECK(ShapeContains(bar, pose, {0.0f, 15.0f}));
        CHECK_FALSE(ShapeContains(bar, pose, {15.0f, 0.0f}));
    }
}
TEST_CASE("Engine.Physics.Contact.PointsFromAToBAndSeparatesThem", "[engine][physics]") {
    const auto circle = Shape::Circle(5.0f);
    const auto touching = ShapeContact(circle, {}, circle, {{8.0f, 0.0f}});
    REQUIRE(touching);
    CHECK(touching->normal.x == Approx(1.0f));
    CHECK(touching->depth == Approx(2.0f));
    CHECK_FALSE(ShapeContact(circle, {}, circle, {{10.5f, 0.0f}}));

    const auto box = Shape::Box(10.0f, 10.0f);
    const Pose near{{13.0f, 0.0f}};
    const auto boxFirst = ShapeContact(box, {}, circle, near);
    REQUIRE(boxFirst);
    CHECK(boxFirst->normal.x == Approx(1.0f));
    CHECK(boxFirst->depth == Approx(2.0f));
    const auto circleFirst = ShapeContact(circle, near, box, {});
    REQUIRE(circleFirst);
    CHECK(circleFirst->normal.x == Approx(-1.0f));
    CHECK(circleFirst->depth == Approx(2.0f));

    const Pose pushed{near.position + boxFirst->normal * (boxFirst->depth + 0.01f)};
    CHECK_FALSE(ShapesOverlap(box, {}, circle, pushed));
}
TEST_CASE("Engine.Physics.Contact.AnswersForEveryPairOfKinds", "[engine][physics]") {
    constexpr std::array<Float2, 3> corners{Float2{-6, -6}, Float2{6, -6}, Float2{0, 6}};
    const std::array shapes{Shape::Circle(5.0f), Shape::Box(5.0f, 5.0f), Shape::Capsule({-5, 0}, {5, 0}, 2.0f),
                            Shape::Segment({-5, 0}, {5, 0}), Shape::Polygon(corners)};
    for (const auto& a : shapes) {
        for (const auto& b : shapes) {
            CHECK(ShapeContact(a, {}, b, {{1.0f, 100.0f}}) == std::nullopt);
            if (a.Kind() == ShapeKind::Segment && b.Kind() == ShapeKind::Segment) continue; // No area to overlap
            const auto contact = ShapeContact(a, {}, b, {{1.0f, 1.0f}});
            REQUIRE(contact);
            CHECK(Length(contact->normal) == Approx(1.0f));
            CHECK(contact->depth >= 0.0f);
        }
    }
}
TEST_CASE("Engine.Physics.Contact.SameCenterStillGivesADirection", "[engine][physics]") {
    const auto contact = ShapeContact(Shape::Circle(5.0f), {}, Shape::Circle(3.0f), {});
    REQUIRE(contact);
    CHECK(Length(contact->normal) == Approx(1.0f));
    CHECK(contact->depth == Approx(8.0f));
}
TEST_CASE("Engine.Physics.Ray.FindsTheSurfaceFacingIt", "[engine][physics]") {
    const auto box = Shape::Box(10.0f, 10.0f);
    const auto hit = CastRay({-100.0f, 0.0f}, {100.0f, 0.0f}, box, {});
    REQUIRE(hit);
    CHECK(hit->fraction == Approx(0.45f).margin(0.001f));
    CHECK(hit->normal.x == Approx(-1.0f));
    CHECK(hit->point.x == Approx(-10.0f).margin(CastSlack));

    CHECK_FALSE(CastRay({-100.0f, 50.0f}, {100.0f, 50.0f}, box, {}));
    CHECK_FALSE(CastRay({-100.0f, 0.0f}, {-50.0f, 0.0f}, box, {})); // Stops short
    CHECK_FALSE(CastRay({100.0f, 0.0f}, {200.0f, 0.0f}, box, {}));  // Moves away
}
TEST_CASE("Engine.Physics.Ray.StartingInsideHitsAtOnce", "[engine][physics]") {
    const auto box = Shape::Box(10.0f, 10.0f);
    const auto hit = CastRay({0.0f, 0.0f}, {100.0f, 0.0f}, box, {});
    REQUIRE(hit);
    CHECK(hit->fraction == 0.0f);
    CHECK(hit->normal == Float2{});

    const auto still = CastRay({0.0f, 0.0f}, {0.0f, 0.0f}, box, {});
    REQUIRE(still);
    CHECK(still->fraction == 0.0f);
    CHECK_FALSE(CastRay({50.0f, 0.0f}, {50.0f, 0.0f}, box, {}));
}
TEST_CASE("Engine.Physics.Sweep.AFastShapeCannotPassThrough", "[engine][physics]") {
    const auto wall = Shape::Segment({0.0f, -50.0f}, {0.0f, 50.0f});
    const auto bullet = Shape::Circle(1.0f);
    CHECK_FALSE(ShapesOverlap(bullet, {{-1000.0f, 0.0f}}, wall, {}));
    CHECK_FALSE(ShapesOverlap(bullet, {{1000.0f, 0.0f}}, wall, {}));

    const auto hit = CastShape(bullet, {{-1000.0f, 0.0f}}, {2000.0f, 0.0f}, wall, {});
    REQUIRE(hit);
    CHECK(hit->fraction == Approx(0.4995f).margin(0.001f));
    CHECK(hit->normal.x == Approx(-1.0f));
    CHECK(hit->point.x == Approx(0.0f).margin(CastSlack));
}
TEST_CASE("Engine.Physics.Sweep.UsesThePoseOfBothShapes", "[engine][physics]") {
    const auto bar = Shape::Box(20.0f, 2.0f);
    const auto target = Shape::Box(1.0f, 1.0f);
    // Upright, the bar reaches y = 11 from y = -9 and misses a target at y = 40 unless it travels down.
    const Pose upright{{0.0f, 0.0f}, Pi / 2.0f};
    const auto hit = CastShape(bar, upright, {0.0f, 100.0f}, target, {{0.0f, 40.0f}});
    REQUIRE(hit);
    CHECK(hit->fraction == Approx(0.19f).margin(0.001f));
    CHECK(hit->normal.y == Approx(-1.0f));

    // A turned target's normal comes back in world coordinates.
    const auto turned = CastRay({-100.0f, 0.0f}, {100.0f, 0.0f}, Shape::Box(10.0f, 10.0f), {{}, Pi / 2.0f});
    REQUIRE(turned);
    CHECK(turned->normal.x == Approx(-1.0f));
    CHECK(turned->normal.y == Approx(0.0f).margin(1e-4f));
    CHECK(turned->point.x == Approx(-10.0f).margin(CastSlack));
}
