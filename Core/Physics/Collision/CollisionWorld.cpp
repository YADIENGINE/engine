#include "Physics/Collision/CollisionWorld.h"
#include <box2d/collision.h>
#include <box2d/math_functions.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>
#include "Physics/Internals/Box2dBridge.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr int InitialCapacity = 16;
// A cast keeps looking this far past the best hit, so a collider at exactly the same distance is seen too.
constexpr float TieMargin = 1e-4f;

struct Slot {
    Shape shape = Shape::Circle(1.0f);
    Pose pose;
    CollisionMask layers;
    std::uint64_t owner = 0;
    int proxy = -1;
    std::uint32_t generation = 1;
    bool alive = false;
};
void RequirePose(const Pose& pose) {
    if (!std::isfinite(pose.position.x) || !std::isfinite(pose.position.y) || !std::isfinite(pose.angle))
        throw std::invalid_argument("CollisionWorld: a pose is not finite");
}
bool CollectCandidate(int, std::uint64_t slot, void* context) {
    static_cast<std::vector<std::uint32_t>*>(context)->push_back(static_cast<std::uint32_t>(slot));
    return true;
}
} // namespace

struct CollisionWorld::Impl {
    Impl()
        : tree(b2CreateDynamicTree(InitialCapacity)) {}
    ~Impl() { b2DestroyDynamicTree(tree); }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Slot& Live(ColliderId id) {
        if (id.generation == 0 || id.index >= slots.size() || !slots[id.index].alive ||
            slots[id.index].generation != id.generation)
            throw std::invalid_argument("CollisionWorld: this collider does not exist");
        return slots[id.index];
    }
    [[nodiscard]] ColliderRef RefOf(std::uint32_t index) const {
        return {{index, slots[index].generation}, slots[index].owner, slots[index].layers};
    }
    // Slots in the box and the mask, ascending. Box2D's mask filter may be approximate, so it is applied again.
    std::vector<std::uint32_t>& Candidates(b2AABB box, CollisionMask mask, ColliderId ignore) {
        candidates.clear();
        (void)b2DynamicTree_OverlapAABB(tree, box, mask.bits, CollectCandidate, &candidates);
        std::erase_if(candidates, [&](std::uint32_t index) { return Skips(index, mask, ignore); });
        std::ranges::sort(candidates);
        return candidates;
    }
    // Whichever is nearer; the lower id at the same distance.
    void Offer(std::uint32_t index, const CastHit& hit) {
        if (best && (hit.fraction > best->fraction || (hit.fraction == best->fraction && index >= best->id.index)))
            return;
        const ColliderRef ref = RefOf(index);
        best = ColliderHit{ref.id, ref.owner, ref.layers, hit.point, hit.normal, hit.fraction};
    }
    [[nodiscard]] float CastLimit(float current) const {
        return best ? std::min(current, best->fraction + TieMargin) : current;
    }
    [[nodiscard]] bool Skips(std::uint32_t index, CollisionMask mask, ColliderId ignore) const {
        return !slots[index].layers.Overlaps(mask) || (ignore.generation != 0 && index == ignore.index);
    }

    struct RayCast {
        Impl* world;
        Float2 from, to;
        CollisionMask mask;
        ColliderId ignore;
    };
    struct ShapeCast {
        Impl* world;
        const Shape* shape;
        const Pose* from;
        Float2 move;
        CollisionMask mask;
        ColliderId ignore;
    };
    static float OnRay(const b2RayCastInput* input, int, std::uint64_t slot, void* context) {
        const auto& cast = *static_cast<RayCast*>(context);
        const auto index = static_cast<std::uint32_t>(slot);
        if (cast.world->Skips(index, cast.mask, cast.ignore)) return input->maxFraction;
        const Slot& target = cast.world->slots[index];
        if (const auto hit = CastRay(cast.from, cast.to, target.shape, target.pose)) cast.world->Offer(index, *hit);
        return cast.world->CastLimit(input->maxFraction);
    }
    static float OnShape(const b2BoxCastInput* input, int, std::uint64_t slot, void* context) {
        const auto& cast = *static_cast<ShapeCast*>(context);
        const auto index = static_cast<std::uint32_t>(slot);
        if (cast.world->Skips(index, cast.mask, cast.ignore)) return input->maxFraction;
        const Slot& target = cast.world->slots[index];
        if (const auto hit = CastShape(*cast.shape, *cast.from, cast.move, target.shape, target.pose))
            cast.world->Offer(index, *hit);
        return cast.world->CastLimit(input->maxFraction);
    }

    b2DynamicTree* tree;
    std::vector<Slot> slots;
    std::vector<std::uint32_t> freeSlots;
    std::size_t count = 0;
    std::vector<std::uint32_t> candidates;
    std::vector<ColliderRef> refs;
    std::vector<ColliderPair> pairs;
    std::optional<ColliderHit> best;
};

CollisionWorld::CollisionWorld()
    : _impl(std::make_unique<Impl>()) {}
CollisionWorld::~CollisionWorld() = default;
CollisionWorld::CollisionWorld(CollisionWorld&&) noexcept = default;
CollisionWorld& CollisionWorld::operator=(CollisionWorld&&) noexcept = default;

ColliderId CollisionWorld::Add(const Shape& shape, const Pose& pose, CollisionMask layers, std::uint64_t owner) {
    RequirePose(pose);
    std::uint32_t index = 0;
    if (_impl->freeSlots.empty()) {
        index = static_cast<std::uint32_t>(_impl->slots.size());
        _impl->slots.emplace_back();
    } else {
        index = _impl->freeSlots.back();
        _impl->freeSlots.pop_back();
    }
    Slot& slot = _impl->slots[index];
    slot.shape = shape;
    slot.pose = pose;
    slot.layers = layers;
    slot.owner = owner;
    slot.alive = true;
    slot.proxy = b2CreateTreeProxy(_impl->tree, BoundsOf(shape, pose), layers.bits, index);
    ++_impl->count;
    return {index, slot.generation};
}
void CollisionWorld::Move(ColliderId id, const Pose& pose) {
    RequirePose(pose);
    Slot& slot = _impl->Live(id);
    if (slot.pose == pose) return;
    slot.pose = pose;
    b2DynamicTree_MoveProxy(_impl->tree, slot.proxy, BoundsOf(slot.shape, pose));
}
void CollisionWorld::SetShape(ColliderId id, const Shape& shape) {
    Slot& slot = _impl->Live(id);
    slot.shape = shape;
    b2DynamicTree_MoveProxy(_impl->tree, slot.proxy, BoundsOf(shape, slot.pose));
}
void CollisionWorld::SetLayers(ColliderId id, CollisionMask layers) {
    Slot& slot = _impl->Live(id);
    slot.layers = layers;
    b2DynamicTree_SetCategoryBits(_impl->tree, slot.proxy, layers.bits);
}
void CollisionWorld::Remove(ColliderId id) {
    Slot& slot = _impl->Live(id);
    b2DestroyTreeProxy(_impl->tree, slot.proxy);
    slot.alive = false;
    slot.proxy = -1;
    ++slot.generation;
    _impl->freeSlots.push_back(id.index);
    --_impl->count;
}
void CollisionWorld::Clear() {
    for (std::uint32_t index = 0; index < _impl->slots.size(); ++index)
        if (_impl->slots[index].alive) Remove({index, _impl->slots[index].generation});
}
bool CollisionWorld::Contains(ColliderId id) const {
    return id.generation != 0 && id.index < _impl->slots.size() && _impl->slots[id.index].alive &&
           _impl->slots[id.index].generation == id.generation;
}
std::size_t CollisionWorld::Count() const { return _impl->count; }
const Pose& CollisionWorld::PoseOf(ColliderId id) const { return _impl->Live(id).pose; }
void CollisionWorld::ForEach(const std::function<void(const ColliderView&)>& visit) const {
    for (std::uint32_t index = 0; index < _impl->slots.size(); ++index) {
        const Slot& slot = _impl->slots[index];
        if (slot.alive) visit(ColliderView{{index, slot.generation}, slot.shape, slot.pose, slot.layers, slot.owner});
    }
}
std::span<const ColliderRef> CollisionWorld::Overlapping(const Shape& shape, const Pose& pose, CollisionMask mask,
                                                         ColliderId ignore) const {
    RequirePose(pose);
    _impl->refs.clear();
    for (const std::uint32_t index : _impl->Candidates(BoundsOf(shape, pose), mask, ignore)) {
        const Slot& slot = _impl->slots[index];
        if (ShapesOverlap(shape, pose, slot.shape, slot.pose)) _impl->refs.push_back(_impl->RefOf(index));
    }
    return _impl->refs;
}
std::span<const ColliderRef> CollisionWorld::At(Float2 point, CollisionMask mask) const {
    _impl->refs.clear();
    const b2Vec2 at = ToBox2d(point);
    for (const std::uint32_t index : _impl->Candidates({at, at}, mask, {})) {
        const Slot& slot = _impl->slots[index];
        if (ShapeContains(slot.shape, slot.pose, point)) _impl->refs.push_back(_impl->RefOf(index));
    }
    return _impl->refs;
}
std::optional<ColliderHit> CollisionWorld::Raycast(Float2 from, Float2 to, CollisionMask mask,
                                                   ColliderId ignore) const {
    _impl->best.reset();
    if (from == to) {
        for (const std::uint32_t index : _impl->Candidates({ToBox2d(from), ToBox2d(from)}, mask, ignore)) {
            const Slot& slot = _impl->slots[index];
            if (const auto hit = CastRay(from, to, slot.shape, slot.pose)) _impl->Offer(index, *hit);
        }
        return _impl->best;
    }
    Impl::RayCast context{_impl.get(), from, to, mask, ignore};
    const b2RayCastInput input{ToBox2d(from), ToBox2d(to - from), 1.0f};
    (void)b2DynamicTree_CastRay(_impl->tree, &input, mask.bits, Impl::OnRay, &context);
    return _impl->best;
}
std::optional<ColliderHit> CollisionWorld::Sweep(const Shape& shape, const Pose& from, Float2 move, CollisionMask mask,
                                                 ColliderId ignore) const {
    RequirePose(from);
    _impl->best.reset();
    if (move == Float2{}) {
        for (const std::uint32_t index : _impl->Candidates(BoundsOf(shape, from), mask, ignore)) {
            const Slot& slot = _impl->slots[index];
            if (const auto hit = CastShape(shape, from, move, slot.shape, slot.pose)) _impl->Offer(index, *hit);
        }
        return _impl->best;
    }
    Impl::ShapeCast context{_impl.get(), &shape, &from, move, mask, ignore};
    const b2BoxCastInput input{BoundsOf(shape, from), ToBox2d(move), 1.0f};
    (void)b2DynamicTree_CastBox(_impl->tree, &input, mask.bits, Impl::OnShape, &context);
    return _impl->best;
}
std::span<const ColliderPair> CollisionWorld::Pairs(CollisionMask first, CollisionMask second) const {
    _impl->pairs.clear();
    for (std::uint32_t index = 0; index < _impl->slots.size(); ++index) {
        const Slot& slot = _impl->slots[index];
        if (!slot.alive || !slot.layers.Overlaps(first)) continue;
        for (const std::uint32_t other :
             _impl->Candidates(BoundsOf(slot.shape, slot.pose), second, {index, slot.generation})) {
            const Slot& candidate = _impl->slots[other];
            // A pair that fits both ways around is reported once.
            if (slot.layers.Overlaps(second) && candidate.layers.Overlaps(first) && other < index) continue;
            if (ShapesOverlap(slot.shape, slot.pose, candidate.shape, candidate.pose))
                _impl->pairs.push_back({_impl->RefOf(index), _impl->RefOf(other)});
        }
    }
    return _impl->pairs;
}
void CollisionWorld::State(StateVisitor& state) const {
    state.Field("colliders", _impl->count);
    ForEach([&](const ColliderView& collider) {
        state.Group("collider", collider.id.index, [&] {
            state.Field("generation", collider.id.generation);
            state.Field("owner", collider.owner);
            state.Field("layers", collider.layers.bits);
            state.Field("kind", collider.shape.Kind());
            state.Field("radius", collider.shape.Radius());
            for (std::size_t i = 0; i < collider.shape.Points().size(); ++i)
                state.Group("point", i, [&] { state.Field("at", collider.shape.Points()[i]); });
            state.Field("position", collider.pose.position);
            state.Field("angle", collider.pose.angle);
        });
    });
}
