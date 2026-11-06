#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>
#include "Physics/Collision/CollisionMask.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Pool.h"

// Where an item stands, for its collider: a member Pose or Float2 (no turn), or a lambda without captures.
template <class PoseOf, class T>
concept PoolColliderPose =
    (std::is_member_object_pointer_v<PoseOf> || (std::is_class_v<PoseOf> && std::is_empty_v<PoseOf>)) &&
    std::regular_invocable<const PoseOf&, const T&> &&
    (std::convertible_to<std::invoke_result_t<const PoseOf&, const T&>, const Pose&> ||
     std::convertible_to<std::invoke_result_t<const PoseOf&, const T&>, Float2>);

// How the items of one Pool<T> have colliders, declared once beside the item type. The world stays an index and the
// game keeps the positions (D27): each item holds its ColliderId, the collider's owner is the item's handle, and its
// layers are the pool's, which is how a hit finds its way back to the pool. Compile-time data only; it points at no
// world or pool, so pass them to each call.
//     struct Enemy { Pose pose; int hp = 3; ColliderId collider; };
//     constexpr PoolColliders EnemyColliders{&Enemy::collider, &Enemy::pose, MaskOf(Layer::Enemy)};
//
//     const Handle<Enemy> enemy = EnemyColliders.Add(_world, _enemies, {.pose = spawn}, Shape::Box(16.0f, 3.0f));
//     EnemyColliders.Sync(_world, _enemies);                      // Each step, after the rules moved the enemies
//     if (const auto hit = _world.Sweep(...))
//         if (Enemy* enemy = _enemies.Find(EnemyColliders.Of(*hit))) enemy->hp -= 1;
//     EnemyColliders.RemoveIf(_world, _enemies, [](const Enemy& e) { return e.hp <= 0; });
// Give each pool its own layers: two pools may give equal handles. An item whose collider is {} has none (one added
// with Pool::Add, or one whose collider the game removed and cleared): Sync and removal skip it. Any other id must
// name a live collider, or the world throws std::invalid_argument.
template <class T, class PoseOf>
    requires PoolColliderPose<PoseOf, T>
class PoolColliders {
public:
    constexpr PoolColliders(ColliderId T::* collider, PoseOf pose, CollisionMask layers)
        : _collider(collider)
        , _pose(pose)
        , _layers(layers) {}

    // Adds the item, then its collider: 'shape' at the item's pose, on the pool's layers, owned by the item's handle.
    Handle<T> Add(CollisionWorld& world, Pool<T>& pool, T item, const Shape& shape) const {
        const Pose pose = PoseFor(item);
        const Handle<T> handle = pool.Add(std::move(item));
        try {
            pool.Get(handle).*_collider = world.Add(shape, pose, _layers, handle.Owner());
        } catch (...) {
            pool.Remove(handle); // An invalid shape or pose leaves no item without its collider
            throw;
        }
        return handle;
    }
    // Moves every collider to its item's pose: one call per step makes a forgotten Move impossible. A pose that did
    // not change costs one comparison.
    void Sync(CollisionWorld& world, const Pool<T>& pool) const {
        for (const T& item : pool)
            if (const ColliderId id = item.*_collider; id != ColliderId{}) world.Move(id, PoseFor(item));
    }
    // Pool::RemoveIf that also removes the leaving items' colliders; returns how many left.
    template <class Predicate>
        requires std::predicate<Predicate&, const T&>
    std::size_t RemoveIf(CollisionWorld& world, Pool<T>& pool, Predicate&& removes) const {
        return pool.RemoveIf([&](const T& item) {
            if (!std::invoke(removes, item)) return false;
            if (const ColliderId id = item.*_collider; id != ColliderId{}) world.Remove(id);
            return true;
        });
    }
    // Pool::Remove that also removes the item's collider.
    void Remove(CollisionWorld& world, Pool<T>& pool, Handle<T> handle) const {
        if (const ColliderId id = pool.Get(handle).*_collider; id != ColliderId{}) world.Remove(id);
        pool.Remove(handle);
    }

    // The handle of the item a found collider belongs to, or null when the collider is on none of the pool's layers
    // (a wall, another pool's thing). Works for ColliderRef, each side of a ColliderPair, and ColliderHit. The item may
    // be gone since: use pool.Find.
    [[nodiscard]] Handle<T> Of(const ColliderRef& found) const { return Of(found.layers, found.owner); }
    [[nodiscard]] Handle<T> Of(const ColliderHit& hit) const { return Of(hit.layers, hit.owner); }

    // The pool's layers, for a query that looks for its items: world.Sweep(..., EnemyColliders.Layers()).
    [[nodiscard]] constexpr CollisionMask Layers() const { return _layers; }

private:
    [[nodiscard]] Handle<T> Of(CollisionMask layers, std::uint64_t owner) const {
        return layers.Overlaps(_layers) ? Handle<T>::FromOwner(owner) : Handle<T>{};
    }
    [[nodiscard]] Pose PoseFor(const T& item) const {
        decltype(auto) where = std::invoke(_pose, item);
        if constexpr (std::convertible_to<decltype(where), const Pose&>)
            return where;
        else
            return Pose{Float2(where)};
    }

    ColliderId T::* _collider;
    PoseOf _pose;
    CollisionMask _layers;
};
