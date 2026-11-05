#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <span>
#include "Animation/Curve.h"
#include "Utilities/Geometry/Float2.h"

// Where a sound is heard from: distance and side. Plain math, with no device and no camera; a view feeds it world
// positions. Header-only, so that a test or a game's own code can ask the same question the player asks.

// How a sound fades with distance, in world units. Full inside `fullWithin`, silent beyond `silentBeyond`, and `curve`
// between them, read from far (0) to near (1). InQuad falls fast at first and slowly later, as distance sounds.
struct SoundFalloff {
    float fullWithin = 160.0f;
    float silentBeyond = 1280.0f;
    Curve curve = Curve::InQuad;
};
[[nodiscard]] constexpr bool IsValid(const SoundFalloff& falloff) {
    return falloff.fullWithin >= 0.0f && falloff.silentBeyond > falloff.fullWithin &&
           falloff.silentBeyond <= std::numeric_limits<float>::max();
}

// Who hears. It is plain data, so a camera can feed it and so can a player's character:
//     sounds.SetListener({.position = view.target, .rotation = view.rotation, .panWidth = view.VisibleArea().width / 2});
struct SoundListener {
    Float2 position{};
    float rotation = 0.0f;       // Radians, clockwise, as CameraView::rotation: what is "right" for this listener
    float panWidth = 640.0f;     // World units to the side at which a sound reaches `maxPan`
    float maxPan = 0.8f;         // Below 1, so that a sound far to one side is not silent in the other ear
    float distanceScale = 1.0f;  // Multiplies distances for the falloff: a zoomed-out view can hear further with < 1
};
[[nodiscard]] inline bool IsValid(const SoundListener& listener) {
    return std::isfinite(listener.position.x) && std::isfinite(listener.position.y) &&
           std::isfinite(listener.rotation) && listener.panWidth > 0.0f &&
           listener.panWidth <= std::numeric_limits<float>::max() && listener.maxPan >= 0.0f &&
           listener.maxPan <= 1.0f && listener.distanceScale >= 0.0f &&
           listener.distanceScale <= std::numeric_limits<float>::max();
}

// What the math decides. `gain` is 0 to 1; `pan` is -1 (left) to 1 (right).
struct SoundPlacement {
    float gain = 1.0f;
    float pan = 0.0f;
};

// The falloff's gain at a distance.
[[nodiscard]] inline float FalloffGain(const SoundFalloff& falloff, float distance) {
    if (distance <= falloff.fullWithin) return 1.0f;
    if (distance >= falloff.silentBeyond) return 0.0f;
    const float closeness = (falloff.silentBeyond - distance) / (falloff.silentBeyond - falloff.fullWithin);
    return std::clamp(Ease(falloff.curve, closeness), 0.0f, 1.0f);
}

[[nodiscard]] inline SoundPlacement PlaceSound(const SoundListener& listener, Float2 source,
                                               const SoundFalloff& falloff = {}) {
    const float dx = source.x - listener.position.x;
    const float dy = source.y - listener.position.y;
    const float side = dx * std::cos(listener.rotation) - dy * std::sin(listener.rotation);
    return {FalloffGain(falloff, std::hypot(dx, dy) * listener.distanceScale),
            std::clamp(side / listener.panWidth, -1.0f, 1.0f) * listener.maxPan};
}

// With several listeners (split screen) the one who hears the sound best decides. None: heard at full, in the middle.
[[nodiscard]] inline SoundPlacement PlaceSound(std::span<const SoundListener> listeners, Float2 source,
                                               const SoundFalloff& falloff = {}) {
    if (listeners.empty()) return {};
    SoundPlacement best = PlaceSound(listeners.front(), source, falloff);
    for (const SoundListener& listener : listeners.subspan(1)) {
        const SoundPlacement placement = PlaceSound(listener, source, falloff);
        if (placement.gain > best.gain) best = placement;
    }
    return best;
}

// The gain of each ear for a pan. Constant power: left² + right² stays 2 wherever the sound is, so it does not get
// louder or quieter as it moves. 1 in both ears in the middle, √2 in one ear at the side; the mix leaves that headroom.
struct SoundEars {
    float left = 1.0f;
    float right = 1.0f;
};
[[nodiscard]] inline SoundEars PanEars(float pan) {
    const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * std::numbers::pi_v<float> / 4.0f;
    return {std::numbers::sqrt2_v<float> * std::cos(angle), std::numbers::sqrt2_v<float> * std::sin(angle)};
}
