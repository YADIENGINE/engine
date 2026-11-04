#pragma once
#include "Utilities/Geometry/Float2.h"

// Moves 'current' towards 'target' like a critically damped spring: fast at first, slowing down, never overshooting.
// 'smoothTime' is roughly the seconds it takes to arrive (0 jumps at once); 'velocity' is the caller's, kept between
// calls. It holds the same course whatever the frame rate, and with a fixed step it is deterministic, so a simulation
// may use it with step.seconds. Game Programming Gems 4, 1.10; Unity calls it SmoothDamp. Header-only.
[[nodiscard]] constexpr float SmoothDamp(float current, float target, float& velocity, float smoothTime,
                                         float seconds) {
    if (!(seconds > 0.0f)) return current;
    if (!(smoothTime > 0.0f)) {
        velocity = 0.0f;
        return target;
    }
    const float omega = 2.0f / smoothTime;
    const float x = omega * seconds;
    const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x); // e^-x, closely
    const float change = current - target;
    const float temp = (velocity + omega * change) * seconds;
    velocity = (velocity - omega * temp) * decay;
    float result = target + (change + temp) * decay;
    if ((target - current > 0.0f) == (result > target)) { // Passed the target: stop on it
        result = target;
        velocity = 0.0f;
    }
    return result;
}
[[nodiscard]] constexpr Float2 SmoothDamp(Float2 current, Float2 target, Float2& velocity, float smoothTime,
                                          float seconds) {
    return {SmoothDamp(current.x, target.x, velocity.x, smoothTime, seconds),
            SmoothDamp(current.y, target.y, velocity.y, smoothTime, seconds)};
}
