#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "Simulation/StateDump.h"
#include "Simulation/StateHash.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Pool.h"
#include "Utilities/Reflection.h"

// Reflected types need a name outside functions and anonymous namespaces (Utilities/Reflection.h).
struct PoolTestEnemy {
    int hp = 3;
    Float2 at{};
    Handle<PoolTestEnemy> target;
};
struct PoolTestBullet {
    float speed = 1.0f;
};
struct PoolTestRun {
    Pool<PoolTestEnemy> enemies;
    int score = 0;
};

namespace {
using Enemies = Pool<PoolTestEnemy>;
template <class P, class H>
concept TakesHandle = requires(const P& pool, H handle) { pool.Contains(handle); };

std::vector<int> Hps(const Enemies& pool) {
    std::vector<int> hps;
    for (const PoolTestEnemy& enemy : pool)
        hps.push_back(enemy.hp);
    return hps;
}
std::uint64_t HashOf(const Enemies& pool) {
    StateHash hash;
    hash.Field("enemies", pool);
    return hash.Result();
}
// A fixed mix of adds and removals.
std::vector<Handle<PoolTestEnemy>> Play(Enemies& pool) {
    std::vector<Handle<PoolTestEnemy>> handles;
    for (int i = 0; i < 6; ++i)
        handles.push_back(pool.Add({.hp = i}));
    pool.Remove(handles[1]);
    (void)pool.RemoveIf([](const PoolTestEnemy& enemy) { return enemy.hp % 2 == 0; });
    handles.push_back(pool.Add({.hp = 10}));
    handles.push_back(pool.Add({.hp = 11}));
    return handles;
}
} // namespace

TEST_CASE("Engine.Pool.HandlesArePlainTypedValues", "[engine][simulation]") {
    STATIC_REQUIRE(std::is_trivially_copyable_v<Handle<PoolTestEnemy>>);
    STATIC_REQUIRE(PlainStruct<Handle<PoolTestEnemy>>);
    STATIC_REQUIRE(PlainStruct<PoolTestEnemy>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<Handle<PoolTestEnemy>, Handle<PoolTestBullet>>);
    STATIC_REQUIRE(TakesHandle<Enemies, Handle<PoolTestEnemy>>);
    STATIC_REQUIRE_FALSE(TakesHandle<Pool<PoolTestBullet>, Handle<PoolTestEnemy>>);
    STATIC_REQUIRE(std::ranges::forward_range<Enemies>);
    STATIC_REQUIRE(std::ranges::forward_range<const Enemies>);
    const Handle<PoolTestEnemy> null;
    CHECK_FALSE(null);
    CHECK(null.Owner() == 0);
    CHECK(Handle<PoolTestEnemy>::FromOwner(0) == null);
}

TEST_CASE("Engine.Pool.SameOperationsGiveSameHandlesAndHash", "[engine][simulation]") {
    Enemies first;
    Enemies second;
    CHECK(Play(first) == Play(second));
    CHECK(HashOf(first) == HashOf(second));
    CHECK(Hps(first) == std::vector<int>{3, 5, 10, 11});
}

TEST_CASE("Engine.Pool.GetFindsTheItemUntilItIsRemoved", "[engine][simulation]") {
    Enemies pool;
    const auto a = pool.Add({.hp = 1});
    const auto b = pool.Add({.hp = 2});
    const auto c = pool.Add({.hp = 3});
    CHECK(pool.Count() == 3);
    pool.Remove(a); // b and c move in the list; their handles still find them
    CHECK(pool.Get(b).hp == 2);
    CHECK(pool.Get(c).hp == 3);
    CHECK_FALSE(pool.Contains(a));
    CHECK(pool.Find(a) == nullptr);
    CHECK_THROWS_AS((void)pool.Get(a), std::invalid_argument);
    CHECK_THROWS_AS(pool.Remove(a), std::invalid_argument);

    // The slot is used again, but the old handle stays stale.
    const auto d = pool.Add({.hp = 4});
    CHECK(d.index == a.index);
    CHECK(d.generation != a.generation);
    CHECK(pool.Find(a) == nullptr);
    CHECK(pool.Get(d).hp == 4);

    // Null, and handles this pool never gave.
    CHECK_THROWS_AS((void)pool.Get({}), std::invalid_argument);
    CHECK(pool.Find({}) == nullptr);
    Enemies other;
    for (int i = 0; i < 10; ++i)
        (void)other.Add({});
    const auto foreign = other.Add({});
    CHECK_FALSE(pool.Contains(foreign));
    CHECK(pool.Find(foreign) == nullptr);
    CHECK_THROWS_AS((void)pool.Get(foreign), std::invalid_argument);
    CHECK(pool.Find(Handle<PoolTestEnemy>{a.index, d.generation + 1}) == nullptr); // A generation not given yet

    pool.Get(d).hp = 40;
    CHECK(pool.Find(d)->hp == 40);
    const Enemies& view = pool;
    CHECK(view.Get(d).hp == 40);
    CHECK(view.Find(b)->hp == 2);
}

TEST_CASE("Engine.Pool.KeepsSpawnOrderThroughRemoval", "[engine][simulation]") {
    Enemies pool;
    std::vector<Handle<PoolTestEnemy>> handles;
    for (int i = 0; i < 8; ++i)
        handles.push_back(pool.Add({.hp = i}));
    CHECK(pool.RemoveIf([](const PoolTestEnemy& enemy) { return enemy.hp % 3 == 0; }) == 3);
    CHECK(Hps(pool) == std::vector<int>{1, 2, 4, 5, 7});
    CHECK(pool.RemoveIf([](const PoolTestEnemy&) { return false; }) == 0);
    for (int i = 0; i < 8; ++i)
        CHECK(pool.Contains(handles[static_cast<std::size_t>(i)]) == (i % 3 != 0));
    (void)pool.Add({.hp = 20}); // A new item goes last, even in a reused slot
    CHECK(Hps(pool) == std::vector<int>{1, 2, 4, 5, 7, 20});
    pool.Remove(handles[4]);
    CHECK(Hps(pool) == std::vector<int>{1, 2, 5, 7, 20});
    for (const auto& handle : handles)
        if (pool.Contains(handle)) CHECK(pool.Get(handle).hp == static_cast<int>(&handle - handles.data()));
}

TEST_CASE("Engine.Pool.RemoveIfAsksEveryItemBeforeAnyLeaves", "[engine][simulation]") {
    Enemies pool;
    const auto leader = pool.Add({.hp = 0});
    for (int i = 1; i <= 3; ++i)
        (void)pool.Add({.hp = i, .target = leader});
    // The leader leaves, and its followers, which still find it while the question runs.
    CHECK(pool.RemoveIf([&](const PoolTestEnemy& enemy) {
        return enemy.hp == 0 || (pool.Contains(enemy.target) && pool.Get(enemy.target).hp == 0 && enemy.hp < 3);
    }) == 3);
    CHECK(Hps(pool) == std::vector<int>{3});
    CHECK(pool.Find((*pool.begin()).target) == nullptr);

    // A question that throws leaves the pool as it was.
    CHECK_THROWS_AS(pool.RemoveIf([](const PoolTestEnemy&) -> bool { throw std::runtime_error("no"); }),
                    std::runtime_error);
    CHECK(pool.Count() == 1);
}

TEST_CASE("Engine.Pool.HandleOfAnItemFromALoop", "[engine][simulation]") {
    Enemies pool;
    std::vector<Handle<PoolTestEnemy>> added;
    for (int i = 0; i < 4; ++i)
        added.push_back(pool.Add({.hp = i}));
    pool.Remove(added[1]);
    std::vector<Handle<PoolTestEnemy>> seen;
    for (const PoolTestEnemy& enemy : pool)
        seen.push_back(pool.HandleOf(enemy));
    CHECK(seen == std::vector<Handle<PoolTestEnemy>>{added[0], added[2], added[3]});
    CHECK(pool.HandleOf(pool.Get(added[3])) == added[3]);
    const PoolTestEnemy outside{};
    CHECK_THROWS_AS((void)pool.HandleOf(outside), std::invalid_argument);
    CHECK_THROWS_AS((void)Enemies{}.HandleOf(outside), std::invalid_argument);
}

TEST_CASE("Engine.Pool.NoAddOrRemoveWhileLooping", "[engine][simulation]") {
    Enemies pool;
    const auto first = pool.Add({.hp = 1});
    (void)pool.Add({.hp = 2});
    int visits = 0;
    for (PoolTestEnemy& enemy : pool) {
        ++visits;
        enemy.hp += 10; // Changing items is fine
        CHECK_THROWS_AS(pool.Add({}), std::logic_error);
        CHECK_THROWS_AS(pool.Remove(first), std::logic_error);
        CHECK_THROWS_AS(pool.RemoveIf([](const PoolTestEnemy&) { return true; }), std::logic_error);
        CHECK(pool.Get(first).hp >= 11); // Reading through handles is fine
    }
    CHECK(visits == 2);
    CHECK_THROWS_AS(pool.RemoveIf([&](const PoolTestEnemy&) {
        (void)pool.Add({});
        return false;
    }),
                    std::logic_error);
    // Once the loops are over, changes are allowed again; std algorithms hold the pool only while they run.
    CHECK(std::ranges::count_if(pool, [](const PoolTestEnemy& enemy) { return enemy.hp > 10; }) == 2);
    {
        auto cursor = pool.begin();
        const auto copy = cursor;
        CHECK(&*copy == &*cursor);
        CHECK_THROWS_AS(pool.Add({}), std::logic_error);
    }
    (void)pool.Add({.hp = 3});
    pool.Remove(first);
    CHECK(Hps(pool) == std::vector<int>{12, 3});
}

TEST_CASE("Engine.Pool.CopiesAreIndependent", "[engine][simulation]") {
    Enemies original;
    const auto a = original.Add({.hp = 1});
    const auto b = original.Add({.hp = 2});
    Enemies copy = original;
    copy.Get(a).hp = 50;
    copy.Remove(b);
    CHECK(original.Get(a).hp == 1);
    CHECK(original.Contains(b));
    CHECK(copy.Get(a).hp == 50); // The same handles name the copy's items
    // A copy made inside a loop is free to change.
    for (const PoolTestEnemy& enemy : original) {
        (void)enemy;
        Enemies inner = original;
        (void)inner.Add({});
        CHECK(inner.Count() == 3);
    }
    Enemies moved = std::move(copy);
    CHECK(moved.Get(a).hp == 50);
    // The same next handle in the original and in a copy of it.
    Enemies again = original;
    CHECK(again.Add({}) == original.Add({}));
}

TEST_CASE("Engine.Pool.StateWritesItemsHandlesAndFreeSlots", "[engine][simulation]") {
    PoolTestRun run;
    const auto a = run.enemies.Add({.hp = 5, .at = {1.0f, 2.0f}});
    const auto b = run.enemies.Add({.hp = 6, .target = a});
    run.enemies.Remove(a);
    StateDump dump;
    dump.Fields(run);
    const auto& result = dump.Result();
    CHECK(result.At("enemies.count").AsInt64() == 1);
    CHECK(result.At("enemies[0].handle.index").AsInt64() == b.index);
    CHECK(result.At("enemies[0].handle.generation").AsInt64() == 1);
    CHECK(result.At("enemies[0].hp").AsInt64() == 6);
    CHECK(result.At("enemies[0].target.index").AsInt64() == a.index);
    CHECK(result.At("enemies.generations[0]").AsInt64() == 2);
    CHECK(result.At("enemies.free[0]").AsInt64() == a.index);
    CHECK(result.At("score").AsInt64() == 0);

    // The same items and handles, but slot 0 was used once more: the next handles differ, and so does the hash.
    Enemies once;
    const auto onceA = once.Add({.hp = 5});
    (void)once.Add({.hp = 6});
    once.Remove(onceA);
    Enemies twice = once;
    twice.Remove(twice.Add({}));
    CHECK(Hps(once) == Hps(twice));
    CHECK(HashOf(once) != HashOf(twice));
    // A different free order changes the next handle too.
    Enemies left;
    Enemies right;
    std::vector<Handle<PoolTestEnemy>> l, r;
    for (int i = 0; i < 3; ++i) {
        l.push_back(left.Add({}));
        r.push_back(right.Add({}));
    }
    left.Remove(l[0]);
    left.Remove(l[1]);
    right.Remove(r[1]);
    right.Remove(r[0]);
    CHECK(Hps(left) == Hps(right));
    CHECK(HashOf(left) != HashOf(right));
    CHECK_FALSE(left.Add({}) == right.Add({}));
}

TEST_CASE("Engine.Pool.HandleRoundTripsThroughAColliderOwner", "[engine][simulation]") {
    const Handle<PoolTestEnemy> handle{7, 3};
    CHECK(Handle<PoolTestEnemy>::FromOwner(handle.Owner()) == handle);
    const Handle<PoolTestEnemy> large{0xFFFFFFFEU, 0xFFFFFFFFU};
    CHECK(Handle<PoolTestEnemy>::FromOwner(large.Owner()) == large);
    CHECK(handle.Owner() != Handle<PoolTestEnemy>{3, 7}.Owner());
}
