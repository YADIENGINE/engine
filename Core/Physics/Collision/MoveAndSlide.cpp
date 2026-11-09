#include "Physics/Collision/MoveAndSlide.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Physics/Internals/NearContact.h"

namespace {
// A move this close to straight into a wall has nothing to slide along: corner assist may look for an opening.
constexpr float HeadOn = 0.995f;
// Places corner assist tries on each side, evenly up to cornerAssist, nearest first.
constexpr int AssistSamples = 8;

bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }

struct Mover {
    const CollisionWorld& world;
    const Shape& shape;
    float angle;
    const SlideOptions& options;

    [[nodiscard]] bool Free(Float2 at, Float2 move) const {
        return !world.Sweep(shape, {at, angle}, move, options.mask, options.ignore);
    }
};

// What is left of a move once a surface with this normal stops it.
Float2 Clip(Float2 move, Float2 normal) {
    const float into = Dot(move, normal);
    return into < 0.0f ? move - normal * into : move;
}

// Straight into a wall beside an opening: steps sideways towards the nearest opening the shape fits through.
bool Assist(const Mover& mover, SlideResult& result, Float2 move) {
    const float length = Length(move);
    const Float2 direction = move * (1.0f / length);
    const Float2 side = Perpendicular(direction);
    const Float2 ahead = direction * std::max(length, mover.options.cornerAssist);
    for (int sample = 1; sample <= AssistSamples; ++sample) {
        const float offset = mover.options.cornerAssist * static_cast<float>(sample) / AssistSamples;
        for (const float sign : {-1.0f, 1.0f}) {
            const Float2 shift = side * (sign * offset);
            if (!mover.Free(result.position, shift) || !mover.Free(result.position + shift, ahead)) continue;
            result.position += side * (sign * std::min(offset, length));
            return true;
        }
    }
    return false;
}
} // namespace

SlideResult MoveAndSlide(const CollisionWorld& world, const Shape& shape, const Pose& from, Float2 move,
                         const SlideOptions& options) {
    if (!std::isfinite(options.skin) || options.skin < MinSlideSkin)
        throw std::invalid_argument("MoveAndSlide: skin is below MinSlideSkin");
    if (!std::isfinite(options.cornerAssist) || options.cornerAssist < 0.0f)
        throw std::invalid_argument("MoveAndSlide: cornerAssist is negative");
    if (!Finite(move)) throw std::invalid_argument("MoveAndSlide: the move is not finite");

    const Mover mover{world, shape, from.angle, options};
    SlideResult result{.position = from.position};
    Float2 remaining = move;
    Float2 previous{};
    for (std::size_t step = 0; step < MaxSlideHits && remaining != Float2{}; ++step) {
        auto hit = world.Sweep(shape, {result.position, from.angle}, remaining, options.mask, options.ignore);
        if (!hit) {
            result.position += remaining;
            remaining = {};
            break;
        }
        if (hit->normal == Float2{}) {
            // Already touching or inside: the collider itself says which way is out.
            const auto contact =
                NearContact(world.ShapeOf(hit->id), world.PoseOf(hit->id), shape, {result.position, from.angle});
            if (!contact) break;
            hit->normal = contact->normal;
            hit->point = contact->point;
            result.position += contact->normal * std::max(0.0f, options.skin - contact->separation);
        } else {
            result.position += remaining * hit->fraction + hit->normal * options.skin;
            remaining = remaining * (1.0f - hit->fraction);
        }
        result.hits[result.hitCount++] = *hit;
        if (Dot(remaining, hit->normal) < 0.0f) result.blocked = true;
        remaining = Clip(remaining, hit->normal);
        // Two walls that meet: sliding along the second one would go back into the first.
        if (Dot(remaining, previous) < 0.0f) remaining = {};
        previous = hit->normal;
    }
    if (remaining != Float2{}) result.blocked = true;

    if (options.cornerAssist > 0.0f && result.hitCount > 0 && move != Float2{} &&
        Dot(Normalized(move), -result.hits[0].normal) >= HeadOn)
        result.assisted = Assist(mover, result, move);
    return result;
}
