#pragma once
#include "Utilities/Float2.h"

// Hedefe yayla yaklaşan 2B nokta (menü imleçleri gibi). Header-only.
struct SpringPoint {
    Float2 position{};
    Float2 velocity{};

    constexpr void Snap(Float2 target) {
        position = target;
        velocity = {};
    }
    // 'maxStep'ten uzun frame'ler kırpılır: takılmadan sonra nokta fırlamaz.
    constexpr void Follow(Float2 target, float deltaSeconds, float stiffness, float damping, float maxStep) {
        const float step = deltaSeconds < maxStep ? deltaSeconds : maxStep;
        velocity.x += ((target.x - position.x) * stiffness - velocity.x * damping) * step;
        velocity.y += ((target.y - position.y) * stiffness - velocity.y * damping) * step;
        position.x += velocity.x * step;
        position.y += velocity.y * step;
    }
};
