#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/PoolColliders.h"
#include "Utilities/Pool.h"

using Catch::Approx;

namespace {
enum class Layer { Enemy, Boss, Crate, Wall };

struct Enemy {
    Float2 at;
    float angle = 0.0f;
    int hp = 1;
    ColliderId collider;
};
struct Crate {
    Pose pose;
    ColliderId collider;
};

constexpr PoolColliders EnemyColliders{&Enemy::collider, &Enemy::at, MaskOf(Layer::Enemy)};
constexpr PoolColliders TurningEnemyColliders{
    &Enemy::collider, [](const Enemy& enemy) { return Pose{enemy.at, enemy.angle}; }, MaskOf(Layer::Enemy)};
constexpr PoolColliders CrateColliders{&Crate::collider, &Crate::pose, MaskOf(Layer::Crate)};
const Shape Ball = Shape::Circle(2.0f);
} // namespace

TEST_CASE("Engine.Physics.PoolColliders.AddPutsTheColliderOnThePoolsLayersAtTheItem", "[engine][physics]") {
    CollisionWorld world;
    Pool<Crate> crates;
    const auto crate = CrateColliders.Add(world, crates, {.pose = {{3.0f, 4.0f}, 0.5f}}, Shape::Box(1.0f, 1.0f));
    const ColliderId id = crates.Get(crate).collider;
    REQUIRE(world.Contains(id));
    CHECK(world.PoseOf(id) == Pose{{3.0f, 4.0f}, 0.5f});
    bool seen = false;
    world.ForEach([&](const ColliderView& collider) {
        seen = true;
        CHECK(collider.layers == MaskOf(Layer::Crate));
        CHECK(collider.owner == crate.Owner());
    });
    CHECK(seen);
    CHECK(CrateColliders.Layers() == MaskOf(Layer::Crate));

    // A pose the world refuses leaves neither an item nor a collider.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(CrateColliders.Add(world, crates, {.pose = {{nan, 0.0f}}}, Ball), std::invalid_argument);
    CHECK(crates.Count() == 1);
    CHECK(world.Count() == 1);
}

TEST_CASE("Engine.Physics.PoolColliders.SyncMovesEveryCollider", "[engine][physics]") {
    CollisionWorld world;
    Pool<Enemy> enemies;
    const auto a = EnemyColliders.Add(world, enemies, {.at = {0.0f, 0.0f}}, Ball);
    const auto b = EnemyColliders.Add(world, enemies, {.at = {10.0f, 0.0f}}, Ball);
    (void)enemies.Add({.at = {50.0f, 50.0f}}); // No collider: skipped
    for (Enemy& enemy : enemies)
        enemy.at.y += 5.0f;
    enemies.Get(b).angle = 1.0f;
    EnemyColliders.Sync(world, enemies);
    CHECK(world.PoseOf(enemies.Get(a).collider).position.y == Approx(5.0f));
    CHECK(world.PoseOf(enemies.Get(b).collider).position.x == Approx(10.0f));
    CHECK(world.PoseOf(enemies.Get(b).collider).angle == 0.0f); // A Float2 gives no turn
    TurningEnemyColliders.Sync(world, enemies);                 // A lambda gives the whole pose
    CHECK(world.PoseOf(enemies.Get(b).collider).angle == Approx(1.0f));

    // A collider removed behind the pool's back is a bug, and the world says so.
    world.Remove(enemies.Get(a).collider);
    CHECK_THROWS_AS(EnemyColliders.Sync(world, enemies), std::invalid_argument);
}

TEST_CASE("Engine.Physics.PoolColliders.OfLeadsAHitBackToItsPool", "[engine][physics]") {
    CollisionWorld world;
    Pool<Enemy> enemies;
    Pool<Crate> crates;
    (void)EnemyColliders.Add(world, enemies, {.at = {0.0f, 0.0f}}, Ball);
    const auto target = EnemyColliders.Add(world, enemies, {.at = {20.0f, 0.0f}, .hp = 7}, Ball);
    // A crate in the same slot as the target gets an equal owner: only the layers tell them apart.
    (void)CrateColliders.Add(world, crates, {.pose = {{-90.0f, 0.0f}}}, Ball);
    const auto crate = CrateColliders.Add(world, crates, {.pose = {{40.0f, 0.0f}}}, Ball);
    CHECK(crate.Owner() == target.Owner());
    (void)world.Add(Shape::Segment({0.0f, 30.0f}, {100.0f, 30.0f}), {}, MaskOf(Layer::Wall));

    const auto all = EnemyColliders.Layers() | CrateColliders.Layers() | MaskOf(Layer::Wall);
    const auto hit = world.Raycast({20.0f, -10.0f}, {20.0f, 40.0f}, all);
    REQUIRE(hit);
    CHECK(EnemyColliders.Of(*hit) == target);
    CHECK_FALSE(CrateColliders.Of(*hit)); // Another pool's layer: null, though the owner number matches one
    CHECK(enemies.Get(EnemyColliders.Of(*hit)).hp == 7);

    const auto found = world.At({40.0f, 0.0f}, all);
    REQUIRE(found.size() == 1);
    CHECK(CrateColliders.Of(found[0]) == crate);
    CHECK_FALSE(EnemyColliders.Of(found[0]));

    const auto wall = world.Raycast({50.0f, 10.0f}, {50.0f, 40.0f}, all);
    REQUIRE(wall);
    CHECK_FALSE(EnemyColliders.Of(*wall));
    CHECK_FALSE(CrateColliders.Of(*wall));

    // A pool on two layers: a hit on either leads back, and so does each side of a pair.
    constexpr PoolColliders BossColliders{&Enemy::collider, &Enemy::at, MaskOf(Layer::Enemy, Layer::Boss)};
    Pool<Enemy> bosses;
    CollisionWorld arena;
    const auto boss = BossColliders.Add(arena, bosses, {.at = {0.0f, 0.0f}}, Ball);
    const auto minion = EnemyColliders.Add(arena, enemies, {.at = {1.0f, 0.0f}}, Ball);
    const auto onBoss = arena.At({0.0f, 0.0f}, MaskOf(Layer::Boss));
    REQUIRE(onBoss.size() == 1);
    CHECK(BossColliders.Of(onBoss[0]) == boss);
    const auto pairs = arena.Pairs(MaskOf(Layer::Boss), MaskOf(Layer::Enemy));
    REQUIRE(pairs.size() == 1);
    CHECK(BossColliders.Of(pairs[0].first) == boss);
    CHECK(EnemyColliders.Of(pairs[0].second) == minion);
    constexpr PoolColliders OnlyBossLayer{&Enemy::collider, &Enemy::at, MaskOf(Layer::Boss)};
    CHECK(OnlyBossLayer.Of(onBoss[0]) == boss); // A collider on two layers overlaps a pool on one of them
}

TEST_CASE("Engine.Physics.PoolColliders.RemovingAnItemRemovesItsCollider", "[engine][physics]") {
    CollisionWorld world;
    Pool<Enemy> enemies;
    std::vector<Handle<Enemy>> spawned;
    for (int i = 0; i < 5; ++i)
        spawned.push_back(
            EnemyColliders.Add(world, enemies, {.at = {static_cast<float>(i) * 10.0f, 0.0f}, .hp = i}, Ball));
    const auto bare = enemies.Add({.hp = 0}); // No collider to remove
    const ColliderId second = enemies.Get(spawned[1]).collider;
    EnemyColliders.Remove(world, enemies, spawned[1]);
    CHECK_FALSE(enemies.Contains(spawned[1]));
    CHECK_FALSE(world.Contains(second));
    CHECK(world.Count() == 4);

    const ColliderId first = enemies.Get(spawned[0]).collider;
    CHECK(EnemyColliders.RemoveIf(world, enemies, [](const Enemy& enemy) { return enemy.hp % 2 == 0; }) ==
          4); // hp 0, 2 and 4, and the bare one
    CHECK_FALSE(enemies.Contains(bare));
    CHECK_FALSE(world.Contains(first));
    CHECK(world.Count() == 1);
    CHECK(enemies.Count() == 1);
    CHECK(world.Contains(enemies.Get(spawned[3]).collider));
    for (const ColliderRef& found : world.At({30.0f, 0.0f}, EnemyColliders.Layers()))
        CHECK(EnemyColliders.Of(found) == spawned[3]);
}

TEST_CASE("Engine.Physics.PoolColliders.ARefusedRemovalLeavesPoolAndWorldTogether", "[engine][physics]") {
    CollisionWorld world;
    Pool<Enemy> enemies;
    const auto a = EnemyColliders.Add(world, enemies, {.at = {0.0f, 0.0f}}, Ball);
    const auto b = EnemyColliders.Add(world, enemies, {.at = {10.0f, 0.0f}, .hp = 0}, Ball);

    // Inside a loop over the pool the pool refuses; the collider must still be there.
    for (const Enemy& enemy : enemies) {
        (void)enemy;
        CHECK_THROWS_AS(EnemyColliders.Remove(world, enemies, a), std::logic_error);
        break;
    }
    CHECK(enemies.Contains(a));
    CHECK(world.Contains(enemies.Get(a).collider));

    // A question that throws after another item was already chosen leaves both unchanged.
    int asked = 0;
    CHECK_THROWS_AS(EnemyColliders.RemoveIf(world, enemies,
                                            [&](const Enemy& enemy) {
                                                if (++asked == 2) throw std::runtime_error("question failed");
                                                return enemy.hp >= 1;
                                            }),
                    std::runtime_error);
    CHECK(enemies.Count() == 2);
    CHECK(world.Count() == 2);
    CHECK(world.Contains(enemies.Get(a).collider));
    CHECK(world.Contains(enemies.Get(b).collider));
}
