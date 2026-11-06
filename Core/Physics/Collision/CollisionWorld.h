#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include "Physics/Collision/Collision.h"
#include "Physics/Collision/CollisionMask.h"
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Geometry/Float2.h"

class StateVisitor;

// Names one collider. A removed collider's id stays invalid, even after its slot is used again.
struct ColliderId {
    std::uint32_t index = 0;
    std::uint32_t generation = 0; // 0: names nothing
    friend constexpr bool operator==(ColliderId, ColliderId) = default;
};

// A collider found by a query. 'owner' is the number given to Add; 'layers' tells which kind of thing that owner
// belongs to. For a pool's items, PoolColliders::Of turns both back into the item's handle (PoolColliders.h).
struct ColliderRef {
    ColliderId id;
    std::uint64_t owner = 0;
    CollisionMask layers;
};
struct ColliderHit {
    ColliderId id;
    std::uint64_t owner = 0;
    CollisionMask layers;
    Float2 point;
    Float2 normal;         // Faces the mover; zero when it started inside (fraction 0)
    float fraction = 0.0f; // 0..1 along the movement
};
struct ColliderPair {
    ColliderRef first;  // From the first mask
    ColliderRef second; // From the second mask
};
// One live collider, for tools and debug drawing.
struct ColliderView {
    ColliderId id;
    const Shape& shape;
    Pose pose;
    CollisionMask layers;
    std::uint64_t owner = 0;
};

// Finds colliders fast when there are many. It is an index, not a physics world: nothing moves by itself, and the game
// keeps its own positions and calls Move when they change. Keep one as a member of the simulation and pass it to
// State, so its colliders appear in dumps beside the game's own positions. Results come in id order, so they do not
// depend on the index's inner shape. Spans stay valid until the next query. An invalid id or pose throws
// std::invalid_argument.
class CollisionWorld {
public:
    CollisionWorld();
    ~CollisionWorld();
    CollisionWorld(CollisionWorld&&) noexcept;
    CollisionWorld& operator=(CollisionWorld&&) noexcept;

    ColliderId Add(const Shape& shape, const Pose& pose, CollisionMask layers, std::uint64_t owner = 0);
    void Move(ColliderId id, const Pose& pose);
    void SetShape(ColliderId id, const Shape& shape);
    void SetLayers(ColliderId id, CollisionMask layers);
    void Remove(ColliderId id);
    void Clear();

    [[nodiscard]] bool Contains(ColliderId id) const;
    [[nodiscard]] std::size_t Count() const;
    [[nodiscard]] const Pose& PoseOf(ColliderId id) const;
    // Every live collider, in id order.
    void ForEach(const std::function<void(const ColliderView&)>& visit) const;

    // Colliders in 'mask' that overlap the shape; pass 'ignore' when asking on behalf of a collider of the world.
    [[nodiscard]] std::span<const ColliderRef> Overlapping(const Shape& shape, const Pose& pose, CollisionMask mask,
                                                           ColliderId ignore = {}) const;
    [[nodiscard]] std::span<const ColliderRef> At(Float2 point, CollisionMask mask) const;
    // The nearest collider along a ray; equal distances go to the lower id.
    [[nodiscard]] std::optional<ColliderHit> Raycast(Float2 from, Float2 to, CollisionMask mask,
                                                     ColliderId ignore = {}) const;
    // The nearest collider a moving shape reaches; it cannot pass through one, however fast it goes.
    [[nodiscard]] std::optional<ColliderHit> Sweep(const Shape& shape, const Pose& from, Float2 move,
                                                   CollisionMask mask, ColliderId ignore = {}) const;
    // Every overlapping pair with one side in each mask, once, in id order. Bullets against enemies:
    //     for (const auto& pair : world.Pairs(MaskOf(Layer::Bullet), MaskOf(Layer::Enemy))) ...
    [[nodiscard]] std::span<const ColliderPair> Pairs(CollisionMask first, CollisionMask second) const;

    // Every collider's fields, for Simulation::State.
    void State(StateVisitor& state) const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};
