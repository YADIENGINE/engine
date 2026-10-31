#pragma once
#include "Utilities/Float2.h"

// 2D point approaching its target with a spring (e.g. menu cursors). Header-only.
struct SpringPoint {
    Float2 position{};
    Float2 velocity{};

    constexpr void Snap(Float2 target) {
        position = target;
        velocity = {};
    }
    // Frames longer than 'maxStep' are clamped so the point does not jump after a stall.
    constexpr void Follow(Float2 target, float deltaSeconds, float stiffness, float damping, float maxStep) {
        const float step = deltaSeconds < maxStep ? deltaSeconds : maxStep;
        velocity.x += ((target.x - position.x) * stiffness - velocity.x * damping) * step;
        velocity.y += ((target.y - position.y) * stiffness - velocity.y * damping) * step;
        position.x += velocity.x * step;
        position.y += velocity.y * step;
    }
};
