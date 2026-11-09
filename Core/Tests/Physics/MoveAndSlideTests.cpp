#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>
#include <stdexcept>
#include "Physics/Collision/MoveAndSlide.h"

using Catch::Approx;

namespace {
enum class Layer { Mover, Wall };
constexpr auto Walls = MaskOf(Layer::Wall);
constexpr float Radius = 4.0f;
const Shape Feet = Shape::Circle(Radius);
constexpr SlideOptions Options{.mask = Walls};
// Where the feet rest on a floor whose top is at y = 10.
constexpr float OnFloor = 10.0f - Radius - 0.05f;

// A floor whose top edge is at y = 10, wide enough for every test.
ColliderId AddFloor(CollisionWorld& world) { return world.Add(Shape::Box(100.0f, 5.0f), {{0.0f, 15.0f}}, Walls, 1); }
// The gap a mover keeps to every wall; negative when it overlaps one.
float Clearance(const CollisionWorld& world, Float2 at) {
    float gap = std::numeric_limits<float>::max();
    world.ForEach([&](const ColliderView& collider) {
        if (collider.layers.Overlaps(Walls))
            gap = std::min(gap, ShapeDistance(Feet, {at}, collider.shape, collider.pose));
    });
    return gap;
}
} // namespace

TEST_CASE("Engine.Physics.Slide.AFreeMoveGoesAllTheWay", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {3.0f, -2.0f}, Options);
    CHECK(result.position == Float2{3.0f, -2.0f});
    CHECK(result.Hits().empty());
    CHECK_FALSE(result.blocked);
}
TEST_CASE("Engine.Physics.Slide.StopsASkinShortOfAWallHeadOn", "[engine][physics]") {
    CollisionWorld world;
    const auto floor = AddFloor(world);
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {0.0f, 20.0f}, Options);
    CHECK(result.position.x == Approx(0.0f).margin(1e-4));
    CHECK(result.position.y == Approx(OnFloor).margin(0.01));
    CHECK(result.blocked);
    REQUIRE(result.Hits().size() == 1);
    CHECK(result.Hits()[0].id == floor);
    CHECK(result.Hits()[0].normal.y == Approx(-1.0f)); // Faces the mover: a floor's normal points up
    CHECK(Clearance(world, result.position) > 0.02f);
}
TEST_CASE("Engine.Physics.Slide.KeepsWhatIsLeftAlongTheWall", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {10.0f, 20.0f}, Options);
    CHECK(result.position.x == Approx(10.0f).margin(0.01));
    CHECK(result.position.y == Approx(OnFloor).margin(0.01));
    CHECK(result.blocked);

    // Already resting on the floor, a move along it is not stopped by it.
    const auto along = MoveAndSlide(world, Feet, {result.position}, {-25.0f, 0.0f}, Options);
    CHECK(along.position.x == Approx(-15.0f).margin(0.01));
    CHECK(along.position.y == Approx(OnFloor).margin(0.01));
}
TEST_CASE("Engine.Physics.Slide.SlidesAlongASlantedWall", "[engine][physics]") {
    CollisionWorld world;
    // A slope rising to the right: its surface runs from (-50, 50) to (50, -50), the solid side below it.
    const std::array<Float2, 3> ramp{Float2{-50.0f, 50.0f}, Float2{50.0f, -50.0f}, Float2{50.0f, 50.0f}};
    (void)world.Add(Shape::Polygon(ramp), {}, Walls, 1);
    const Float2 start{-20.0f, 0.0f};
    const auto result = MoveAndSlide(world, Feet, {start}, {20.0f, 0.0f}, Options);
    CHECK(result.blocked);
    CHECK(Clearance(world, result.position) > 0.02f);
    // It went up the slope rather than stopping where it first met it.
    CHECK(result.position.x > -10.0f);
    CHECK(result.position.y < 0.0f);
    const Float2 normal = result.Hits()[0].normal;
    CHECK(normal.x == Approx(-0.7071f).margin(1e-3));
    CHECK(normal.y == Approx(-0.7071f).margin(1e-3));
}
TEST_CASE("Engine.Physics.Slide.StopsInACorner", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    (void)world.Add(Shape::Box(5.0f, 100.0f), {{15.0f, 0.0f}}, Walls, 2); // A wall whose left edge is at x = 10
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {20.0f, 20.0f}, Options);
    CHECK(result.position.x == Approx(OnFloor).margin(0.01));
    CHECK(result.position.y == Approx(OnFloor).margin(0.01));
    CHECK(result.blocked);
    CHECK(result.Hits().size() == 2);
}
TEST_CASE("Engine.Physics.Slide.NeverPassesThroughAThinWall", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Shape::Segment({-100.0f, 10.0f}, {100.0f, 10.0f}), {}, Walls, 1);
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {1.0f, 10000.0f}, Options);
    CHECK(result.position.y < 10.0f - Radius);
    CHECK(result.blocked);
}
TEST_CASE("Engine.Physics.Slide.IgnoresTheMoversOwnCollider", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    const auto self = world.Add(Feet, {{0.0f, 0.0f}}, Walls, 9);
    CHECK_FALSE(MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {5.0f, 0.0f}, Options).Hits().empty());
    SlideOptions options = Options;
    options.ignore = self;
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {5.0f, 0.0f}, options);
    CHECK_FALSE(result.blocked);
    CHECK(result.position == Float2{5.0f, 0.0f});
}
TEST_CASE("Engine.Physics.Slide.AMoverInsideAWallIsPushedOutFirst", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    const auto result = MoveAndSlide(world, Feet, {{0.0f, 8.0f}}, {5.0f, 0.0f}, Options); // 2 units into the floor
    CHECK(result.position.x == Approx(5.0f).margin(0.01));
    CHECK(result.position.y == Approx(OnFloor).margin(0.01));
    CHECK(Clearance(world, result.position) > 0.02f);
}
TEST_CASE("Engine.Physics.Slide.CornerAssistFindsTheDoorway", "[engine][physics]") {
    CollisionWorld world;
    // A wall along y = 10..20 with a doorway 12 units wide between x = -6 and x = 6.
    (void)world.Add(Shape::Box(47.0f, 5.0f), {{-53.0f, 15.0f}}, Walls, 1);
    (void)world.Add(Shape::Box(47.0f, 5.0f), {{53.0f, 15.0f}}, Walls, 2);
    const Pose start{{7.0f, 0.0f}}; // The feet's center is above the wall: nothing to slip round

    const auto without = MoveAndSlide(world, Feet, start, {0.0f, 20.0f}, Options);
    CHECK(without.position.x == Approx(7.0f).margin(1e-4));
    CHECK(without.position.y == Approx(OnFloor).margin(0.01));
    CHECK_FALSE(without.assisted);

    SlideOptions options = Options;
    options.cornerAssist = 6.0f;
    const auto with = MoveAndSlide(world, Feet, start, {0.0f, 20.0f}, options);
    CHECK(with.assisted);
    CHECK(with.position.x < 6.0f - Radius);
    CHECK(with.position.x > 0.0f);
    // Next step it walks through.
    const auto through = MoveAndSlide(world, Feet, {with.position}, {0.0f, 20.0f}, options);
    CHECK_FALSE(through.blocked);

    // At most as far as the move: a slow walker drifts towards the doorway over several steps.
    const auto slow = MoveAndSlide(world, Feet, {without.position}, {0.0f, 0.25f}, options);
    CHECK(slow.assisted);
    CHECK(slow.position.x == Approx(6.75f).margin(1e-4));

    // A move that is not straight into the wall slides instead; assist leaves it alone.
    const auto slanted = MoveAndSlide(world, Feet, start, {8.0f, 20.0f}, options);
    CHECK_FALSE(slanted.assisted);
}
TEST_CASE("Engine.Physics.Slide.ARoundMoverSlipsPastACornerItBarelyMeets", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Shape::Box(47.0f, 5.0f), {{-53.0f, 15.0f}}, Walls, 1);
    (void)world.Add(Shape::Box(47.0f, 5.0f), {{53.0f, 15.0f}}, Walls, 2);
    // 1 unit too far right for the doorway: the corner turns the feet aside, no assist needed.
    const auto result = MoveAndSlide(world, Feet, {{3.0f, 0.0f}}, {0.0f, 20.0f}, Options);
    CHECK(result.position.y > 10.0f);
    CHECK(result.position.x < 6.0f - Radius);
    CHECK(Clearance(world, result.position) > 0.0f);
}
TEST_CASE("Engine.Physics.Slide.SameInputSameResult", "[engine][physics]") {
    CollisionWorld world;
    AddFloor(world);
    (void)world.Add(Shape::Circle(6.0f), {{12.0f, 4.0f}}, Walls, 2);
    const auto first = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {30.0f, 17.0f}, Options);
    const auto second = MoveAndSlide(world, Feet, {{0.0f, 0.0f}}, {30.0f, 17.0f}, Options);
    CHECK(first.position == second.position);
    CHECK(first.hitCount == second.hitCount);
    CHECK(Clearance(world, first.position) > 0.0f);
}
TEST_CASE("Engine.Physics.Slide.RejectsBadOptions", "[engine][physics]") {
    CollisionWorld world;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    SlideOptions thin = Options;
    thin.skin = 0.01f;
    CHECK_THROWS_AS((void)MoveAndSlide(world, Feet, {}, {1.0f, 0.0f}, thin), std::invalid_argument);
    SlideOptions negative = Options;
    negative.cornerAssist = -1.0f;
    CHECK_THROWS_AS((void)MoveAndSlide(world, Feet, {}, {1.0f, 0.0f}, negative), std::invalid_argument);
    CHECK_THROWS_AS((void)MoveAndSlide(world, Feet, {}, {nan, 0.0f}, Options), std::invalid_argument);
}
