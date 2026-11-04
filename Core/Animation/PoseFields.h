#pragma once
#include <concepts>
#include <string_view>
#include <tuple>
#include "Animation/AnimationValue.h"

// A pose is a game's own plain struct of what animates: an arm's angle, a scale, a tint, a sprite frame. It lists its
// fields once, and blending and State follow from the list:
//
//     struct BossPose {
//         float arm = 0.0f;
//         Float2 offset{};
//         static constexpr auto Fields() {
//             return PoseFields(PoseField("arm", &BossPose::arm), PoseField("offset", &BossPose::offset));
//         }
//     };
//
// Default values are the rest pose: what a clip with no track for a field leaves it at. A field missing from the list
// does not blend and does not reach State, so list them all.
template <class PoseType, class T> struct PoseFieldEntry {
    std::string_view name;
    T PoseType::*member;
};
template <class PoseType, class T>
[[nodiscard]] constexpr PoseFieldEntry<PoseType, T> PoseField(std::string_view name, T PoseType::*member) {
    return {name, member};
}
template <class... Entries> [[nodiscard]] constexpr auto PoseFields(Entries... entries) {
    return std::tuple<Entries...>(entries...);
}

template <class P>
concept AnimationPose = std::copyable<P> && std::default_initializable<P> && requires { P::Fields(); };

// Every listed field from a to b by t.
template <AnimationPose P> [[nodiscard]] P MixPose(const P& a, const P& b, float t) {
    P mixed = a;
    std::apply([&](const auto&... entries) { ((mixed.*entries.member = MixValue(a.*entries.member, b.*entries.member, t)), ...); },
               P::Fields());
    return mixed;
}
// Every listed field, for a simulation's State.
template <AnimationPose P, class Visitor> void VisitPose(Visitor& state, const P& pose) {
    std::apply([&](const auto&... entries) { (state.Field(entries.name, pose.*entries.member), ...); }, P::Fields());
}
