#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/StateHash.h"

using Catch::Approx;

namespace {
enum class Layer { Player, Enemy, Bullet, Wall };
constexpr auto Enemies = MaskOf(Layer::Enemy);
const Shape Ball = Shape::Circle(1.0f);

std::vector<std::uint64_t> Owners(std::span<const ColliderRef> found) {
    std::vector<std::uint64_t> owners;
    for (const auto& item : found)
        owners.push_back(item.owner);
    return owners;
}
std::uint64_t HashOf(const CollisionWorld& world) {
    StateHash hash;
    world.State(hash);
    return hash.Result();
}
} // namespace

TEST_CASE("Engine.Physics.World.FindsOnlyWhatTheMaskAsksFor", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Ball, {{0.0f, 0.0f}}, MaskOf(Layer::Enemy), 1);
    (void)world.Add(Ball, {{0.5f, 0.0f}}, MaskOf(Layer::Wall), 2);
    (void)world.Add(Ball, {{50.0f, 0.0f}}, MaskOf(Layer::Enemy), 3);
    CHECK(world.Count() == 3);

    CHECK(Owners(world.Overlapping(Ball, {}, Enemies)) == std::vector<std::uint64_t>{1});
    CHECK(Owners(world.Overlapping(Ball, {}, MaskOf(Layer::Wall))) == std::vector<std::uint64_t>{2});
    CHECK(Owners(world.Overlapping(Ball, {}, MaskOf(Layer::Enemy, Layer::Wall))) == std::vector<std::uint64_t>{1, 2});
    CHECK(Owners(world.Overlapping(Ball, {}, CollisionMask::All())) == std::vector<std::uint64_t>{1, 2});
    CHECK(world.Overlapping(Ball, {}, MaskOf(Layer::Bullet)).empty());
    CHECK(Owners(world.At({50.0f, 0.0f}, Enemies)) == std::vector<std::uint64_t>{3});
    CHECK(world.At({50.0f, 0.0f}, MaskOf(Layer::Wall)).empty());
    CHECK(world.At({25.0f, 0.0f}, CollisionMask::All()).empty());
}
TEST_CASE("Engine.Physics.World.TheExactShapeDecidesNotItsBox", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Shape::Circle(10.0f), {}, Enemies, 1);
    CHECK(world.At({9.0f, 0.0f}, Enemies).size() == 1);
    CHECK(world.At({9.0f, 9.0f}, Enemies).empty()); // Inside its bounding box, outside the circle
}
TEST_CASE("Engine.Physics.World.AColliderCanIgnoreItself", "[engine][physics]") {
    CollisionWorld world;
    const auto self = world.Add(Ball, {}, Enemies, 1);
    (void)world.Add(Ball, {{1.0f, 0.0f}}, Enemies, 2);
    CHECK(Owners(world.Overlapping(Ball, {}, Enemies, self)) == std::vector<std::uint64_t>{2});
    const auto hit = world.Raycast({-10.0f, 0.0f}, {10.0f, 0.0f}, Enemies, self);
    REQUIRE(hit);
    CHECK(hit->owner == 2);
    const auto swept = world.Sweep(Ball, {{-10.0f, 0.0f}}, {20.0f, 0.0f}, Enemies, self);
    REQUIRE(swept);
    CHECK(swept->owner == 2);
}
TEST_CASE("Engine.Physics.World.MoveAndChangesReachQueries", "[engine][physics]") {
    CollisionWorld world;
    const auto id = world.Add(Ball, {}, Enemies, 1);
    world.Move(id, {{100.0f, 0.0f}});
    CHECK(world.PoseOf(id).position == Float2{100.0f, 0.0f});
    CHECK(world.At({0.0f, 0.0f}, Enemies).empty());
    CHECK(world.At({100.0f, 0.0f}, Enemies).size() == 1);

    world.SetShape(id, Shape::Circle(20.0f));
    CHECK(world.At({115.0f, 0.0f}, Enemies).size() == 1);

    world.SetLayers(id, MaskOf(Layer::Wall));
    CHECK(world.At({100.0f, 0.0f}, Enemies).empty());
    CHECK(world.At({100.0f, 0.0f}, MaskOf(Layer::Wall)).size() == 1);
}
TEST_CASE("Engine.Physics.World.ARemovedIdStaysInvalid", "[engine][physics]") {
    CollisionWorld world;
    const auto old = world.Add(Ball, {}, Enemies, 1);
    world.Remove(old);
    CHECK_FALSE(world.Contains(old));
    CHECK(world.Count() == 0);
    CHECK(world.At({}, Enemies).empty());
    CHECK_THROWS_AS(world.Move(old, {}), std::invalid_argument);
    CHECK_THROWS_AS(world.Remove(old), std::invalid_argument);

    const auto reused = world.Add(Ball, {}, Enemies, 2);
    CHECK(reused.index == old.index);
    CHECK_FALSE(reused == old);
    CHECK(world.Contains(reused));
    CHECK_FALSE(world.Contains(old));
    CHECK_FALSE(world.Contains(ColliderId{}));
    CHECK_THROWS_AS(world.Move(ColliderId{}, {}), std::invalid_argument);
}
TEST_CASE("Engine.Physics.World.RefusesAPoseThatIsNotANumber", "[engine][physics]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CollisionWorld world;
    CHECK_THROWS_AS(world.Add(Ball, {{nan, 0.0f}}, Enemies), std::invalid_argument);
    const auto id = world.Add(Ball, {}, Enemies);
    CHECK_THROWS_AS(world.Move(id, {{}, nan}), std::invalid_argument);
    CHECK_THROWS_AS((void)world.Overlapping(Ball, {{nan, 0.0f}}, Enemies), std::invalid_argument);
    CHECK_THROWS_AS((void)world.Sweep(Ball, {{nan, 0.0f}}, {1.0f, 0.0f}, Enemies), std::invalid_argument);
    CHECK(world.Count() == 1);
}
TEST_CASE("Engine.Physics.World.ClearEmptiesAndForEachVisitsInOrder", "[engine][physics]") {
    CollisionWorld world;
    const auto first = world.Add(Ball, {{1.0f, 0.0f}}, Enemies, 10);
    (void)world.Add(Shape::Box(1.0f, 1.0f), {{2.0f, 0.0f}}, MaskOf(Layer::Wall), 20);
    std::vector<std::uint64_t> owners;
    world.ForEach([&](const ColliderView& collider) { owners.push_back(collider.owner); });
    CHECK(owners == std::vector<std::uint64_t>{10, 20});

    world.Clear();
    CHECK(world.Count() == 0);
    CHECK_FALSE(world.Contains(first));
    CHECK(world.At({1.0f, 0.0f}, CollisionMask::All()).empty());
}
TEST_CASE("Engine.Physics.World.RaycastAndSweepTakeTheNearest", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Ball, {{30.0f, 0.0f}}, Enemies, 3);
    (void)world.Add(Ball, {{10.0f, 0.0f}}, Enemies, 1);
    (void)world.Add(Ball, {{20.0f, 0.0f}}, Enemies, 2);
    (void)world.Add(Ball, {{5.0f, 0.0f}}, MaskOf(Layer::Wall), 9);

    const auto ray = world.Raycast({0.0f, 0.0f}, {100.0f, 0.0f}, Enemies);
    REQUIRE(ray);
    CHECK(ray->owner == 1);
    CHECK(ray->fraction == Approx(0.09f).margin(0.001f));
    CHECK(ray->normal.x == Approx(-1.0f));

    const auto sweep = world.Sweep(Ball, {{0.0f, 0.0f}}, {100.0f, 0.0f}, Enemies);
    REQUIRE(sweep);
    CHECK(sweep->owner == 1);
    CHECK(sweep->fraction == Approx(0.08f).margin(0.001f));

    CHECK_FALSE(world.Raycast({0.0f, 50.0f}, {100.0f, 50.0f}, Enemies));
    CHECK_FALSE(world.Sweep(Ball, {{0.0f, 50.0f}}, {100.0f, 0.0f}, Enemies));
    CHECK_FALSE(world.Raycast({0.0f, 0.0f}, {100.0f, 0.0f}, MaskOf(Layer::Bullet)));
}
TEST_CASE("Engine.Physics.World.EqualDistancesGoToTheLowerId", "[engine][physics]") {
    CollisionWorld world;
    const auto low = world.Add(Shape::Box(1.0f, 20.0f), {{10.0f, 0.0f}}, Enemies, 1);
    (void)world.Add(Shape::Box(1.0f, 20.0f), {{10.0f, 0.0f}}, Enemies, 2);
    const auto ray = world.Raycast({0.0f, 0.0f}, {100.0f, 0.0f}, Enemies);
    REQUIRE(ray);
    CHECK(ray->id == low);
    const auto sweep = world.Sweep(Ball, {{0.0f, 0.0f}}, {100.0f, 0.0f}, Enemies);
    REQUIRE(sweep);
    CHECK(sweep->id == low);
}
TEST_CASE("Engine.Physics.World.ANonMovingCastAsksWhatIsAlreadyThere", "[engine][physics]") {
    CollisionWorld world;
    (void)world.Add(Ball, {}, Enemies, 1);
    const auto ray = world.Raycast({0.5f, 0.0f}, {0.5f, 0.0f}, Enemies);
    REQUIRE(ray);
    CHECK(ray->owner == 1);
    CHECK(ray->fraction == 0.0f);
    const auto sweep = world.Sweep(Ball, {{1.0f, 0.0f}}, {}, Enemies);
    REQUIRE(sweep);
    CHECK(sweep->fraction == 0.0f);
    CHECK_FALSE(world.Raycast({50.0f, 0.0f}, {50.0f, 0.0f}, Enemies));
    CHECK_FALSE(world.Sweep(Ball, {{50.0f, 0.0f}}, {}, Enemies));
}
TEST_CASE("Engine.Physics.World.ASweepCannotPassThroughManyThinWalls", "[engine][physics]") {
    CollisionWorld world;
    for (int i = 0; i < 100; ++i) {
        const float x = 500.0f + static_cast<float>(i) * 10.0f;
        (void)world.Add(Shape::Segment({0.0f, -50.0f}, {0.0f, 50.0f}), {{x, 0.0f}}, MaskOf(Layer::Wall),
                        static_cast<std::uint64_t>(i));
    }
    const auto hit = world.Sweep(Ball, {{0.0f, 0.0f}}, {5000.0f, 0.0f}, MaskOf(Layer::Wall));
    REQUIRE(hit);
    CHECK(hit->owner == 0);
}
TEST_CASE("Engine.Physics.World.PairsComeOnceInIdOrder", "[engine][physics]") {
    CollisionWorld world;
    const auto enemyA = world.Add(Ball, {{0.0f, 0.0f}}, MaskOf(Layer::Enemy), 1);
    const auto bulletA = world.Add(Ball, {{0.5f, 0.0f}}, MaskOf(Layer::Bullet), 2);
    const auto enemyB = world.Add(Ball, {{100.0f, 0.0f}}, MaskOf(Layer::Enemy), 3);
    const auto bulletB = world.Add(Ball, {{100.5f, 0.0f}}, MaskOf(Layer::Bullet), 4);
    (void)world.Add(Ball, {{200.0f, 0.0f}}, MaskOf(Layer::Bullet), 5);

    const auto across = world.Pairs(MaskOf(Layer::Bullet), MaskOf(Layer::Enemy));
    REQUIRE(across.size() == 2);
    CHECK(across[0].first.id == bulletA);
    CHECK(across[0].second.id == enemyA);
    CHECK(across[1].first.id == bulletB);
    CHECK(across[1].second.id == enemyB);

    const auto within = world.Pairs(CollisionMask::All(), CollisionMask::All());
    REQUIRE(within.size() == 2);
    CHECK(within[0].first.id == enemyA);
    CHECK(within[0].second.id == bulletA);
    CHECK(within[1].first.id == enemyB);
    CHECK(within[1].second.id == bulletB);
}
TEST_CASE("Engine.Physics.World.ResultsDoNotDependOnTheIndexShape", "[engine][physics]") {
    const auto fill = [](CollisionWorld& world) {
        std::vector<ColliderId> ids;
        for (int i = 0; i < 64; ++i)
            ids.push_back(world.Add(Ball, {{static_cast<float>(i % 8), static_cast<float>(i / 8)}}, Enemies,
                                    static_cast<std::uint64_t>(i)));
        return ids;
    };
    CollisionWorld straight, wandered;
    (void)fill(straight);
    const auto ids = fill(wandered);
    for (int round = 0; round < 3; ++round) { // Reinserting changes the tree, not the colliders
        for (std::size_t i = 0; i < ids.size(); ++i) {
            const auto home = wandered.PoseOf(ids[i]);
            wandered.Move(ids[i], {{home.position.x + 1000.0f, home.position.y}});
            wandered.Move(ids[i], home);
        }
    }
    const auto area = Shape::Box(4.0f, 4.0f);
    const Pose middle{{4.0f, 4.0f}};
    CHECK(Owners(straight.Overlapping(area, middle, Enemies)) == Owners(wandered.Overlapping(area, middle, Enemies)));
    CHECK(HashOf(straight) == HashOf(wandered));
    const auto a = straight.Pairs(Enemies, Enemies);
    std::vector<std::uint64_t> firstOwners;
    for (const auto& pair : a)
        firstOwners.push_back(pair.first.owner * 100U + pair.second.owner);
    const auto b = wandered.Pairs(Enemies, Enemies);
    std::vector<std::uint64_t> secondOwners;
    for (const auto& pair : b)
        secondOwners.push_back(pair.first.owner * 100U + pair.second.owner);
    CHECK_FALSE(firstOwners.empty());
    CHECK(firstOwners == secondOwners);
}
TEST_CASE("Engine.Physics.World.StateSeesEveryChange", "[engine][physics]") {
    CollisionWorld a, b;
    const auto idA = a.Add(Ball, {}, Enemies, 1);
    const auto idB = b.Add(Ball, {}, Enemies, 1);
    CHECK(HashOf(a) == HashOf(b));
    a.Move(idA, {{1.0f, 0.0f}});
    CHECK(HashOf(a) != HashOf(b));
    b.Move(idB, {{1.0f, 0.0f}});
    CHECK(HashOf(a) == HashOf(b));
    a.SetLayers(idA, MaskOf(Layer::Wall));
    CHECK(HashOf(a) != HashOf(b));
    b.SetLayers(idB, MaskOf(Layer::Wall));
    a.SetShape(idA, Shape::Circle(2.0f));
    CHECK(HashOf(a) != HashOf(b));
}
TEST_CASE("Engine.Physics.World.CanBeMoved", "[engine][physics]") {
    CollisionWorld first;
    const auto id = first.Add(Ball, {}, Enemies, 7);
    CollisionWorld second = std::move(first);
    CHECK(second.Contains(id));
    CollisionWorld third;
    third = std::move(second);
    CHECK(Owners(third.At({}, Enemies)) == std::vector<std::uint64_t>{7});
}
