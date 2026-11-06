#pragma once
#include <concepts>
#include <string_view>
#include "Animation/AnimationValue.h"
#include "Utilities/Reflection.h"

// A pose is a game's own plain struct of what animates: an arm's angle, a scale, a tint, a sprite frame. Every field
// blends and reaches State, by its own name and in declaration order; there is no list to keep:
//
//     struct BossPose {
//         float arm = 0.0f;
//         Float2 offset{};
//         int frame = 0; // Held: changes at the middle of a blend
//     };
//
// Default values are the rest pose: what a clip with no track for a field leaves it at. Fields are numbers, enums,
// bools, types with a Mix (Float2, Color4) or other poses. A pose is a plain struct (Utilities/Reflection.h): public
// fields, no constructor or base class, declared outside any function and anonymous namespace. Something that should
// not animate does not belong in the pose; keep it beside it.
template <class P>
concept AnimationPose = std::copyable<P> && std::default_initializable<P> && PlainStruct<P> && Animatable<P>;

// Every field from a to b by t.
template <AnimationPose P> [[nodiscard]] P MixPose(const P& a, const P& b, float t) { return MixFields(a, b, t); }
// Every field, for a simulation's State.
template <AnimationPose P, class Visitor> void VisitPose(Visitor& state, const P& pose) {
    ForEachField(pose, [&](std::string_view name, const auto& field) { state.Field(name, field); });
}
