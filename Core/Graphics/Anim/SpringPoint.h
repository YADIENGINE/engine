#pragma once
#include <raylib.h>

// Hedefe yayla yaklaşan 2B nokta (menü imleçleri gibi). Header-only.
struct SpringPoint {
    Vector2 position{};
    Vector2 velocity{};

    constexpr void Snap(Vector2 target) {
        position = target;
        velocity = {};
    }
    // 'maxStep'ten uzun frame'ler kırpılır: takılmadan sonra nokta fırlamaz.
    constexpr void Follow(Vector2 target, float deltaSeconds, float stiffness, float damping, float maxStep) {
        const float step = deltaSeconds < maxStep ? deltaSeconds : maxStep;
        velocity.x += ((target.x - position.x) * stiffness - velocity.x * damping) * step;
        velocity.y += ((target.y - position.y) * stiffness - velocity.y * damping) * step;
        position.x += velocity.x * step;
        position.y += velocity.y * step;
    }
};
