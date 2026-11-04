#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "Animation/SmoothDamp.h"

using Catch::Approx;

TEST_CASE("Engine.Animation.SmoothDamp.ArrivesWithoutOvershoot", "[engine][animation]") {
    float value = 0.0f, velocity = 0.0f, previous = 0.0f;
    for (int frame = 0; frame < 120; ++frame) {
        value = SmoothDamp(value, 10.0f, velocity, 0.25f, 1.0f / 60.0f);
        CHECK(value >= previous);
        CHECK(value <= 10.0f);
        previous = value;
    }
    CHECK(value == Approx(10.0f).margin(1e-3f));
    // Going down, and with a target that jumps past the value.
    value = SmoothDamp(value, -5.0f, velocity, 0.25f, 1.0f / 60.0f);
    CHECK(value < 10.0f);
    CHECK(value > -5.0f);
}
TEST_CASE("Engine.Animation.SmoothDamp.EdgeCases", "[engine][animation]") {
    float velocity = 3.0f;
    CHECK(SmoothDamp(1.0f, 9.0f, velocity, 0.0f, 0.1f) == 9.0f); // No smoothing: at once
    CHECK(velocity == 0.0f);
    velocity = 3.0f;
    CHECK(SmoothDamp(1.0f, 9.0f, velocity, 0.5f, 0.0f) == 1.0f); // No time: nothing moves
    CHECK(velocity == 3.0f);
    Float2 velocity2{};
    const Float2 moved = SmoothDamp(Float2{0.0f, 0.0f}, Float2{10.0f, -10.0f}, velocity2, 0.2f, 0.05f);
    CHECK(moved.x == Approx(-moved.y));
    CHECK(moved.x > 0.0f);
}
