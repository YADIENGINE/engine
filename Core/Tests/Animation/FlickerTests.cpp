#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include "Animation/Flicker.h"

namespace {
constexpr double Step = 1.0 / 120.0;
constexpr int Samples = 120 * 60; // A minute at 120 frames a second

// The largest change between two frames.
float LargestJump(const FlickerStyle& style, unsigned seed, bool flashes) {
    float largest = 0.0f;
    float last = Flicker(0.0, style, seed, flashes);
    for (int i = 1; i < Samples; ++i) {
        const float now = Flicker(static_cast<double>(i) * Step, style, seed, flashes);
        largest = std::max(largest, std::abs(now - last));
        last = now;
    }
    return largest;
}
} // namespace

TEST_CASE("Engine.Flicker.TheSameTimeGivesTheSameValue", "[engine][animation]") {
    for (const bool flashes : {true, false})
        for (const double seconds : {0.0, 1.25, 77.7})
            CHECK(Flicker(seconds, FlickerStyle::DeadFluorescent(), 3U, flashes) ==
                  Flicker(seconds, FlickerStyle::DeadFluorescent(), 3U, flashes));
}

TEST_CASE("Engine.Flicker.StaysInRange", "[engine][animation]") {
    FlickerStyle wild = FlickerStyle::DeadFluorescent();
    wild.depth = 5.0f;
    wild.stutterLevel = -3.0f;
    for (const FlickerStyle& style : {FlickerStyle::Candle(), FlickerStyle::DeadFluorescent(), wild})
        for (int i = 0; i < Samples; ++i) {
            const float value = Flicker(static_cast<double>(i) * Step, style, 1U, true);
            REQUIRE(value >= 0.0f);
            REQUIRE(value <= 1.0f);
        }
}

TEST_CASE("Engine.Flicker.SeedsKeepLampsApart", "[engine][animation]") {
    int different = 0;
    for (int i = 0; i < 100; ++i) {
        const double seconds = static_cast<double>(i) * 0.17;
        if (Flicker(seconds, FlickerStyle::Candle(), 1U, true) != Flicker(seconds, FlickerStyle::Candle(), 2U, true))
            ++different;
    }
    CHECK(different > 90);
}

TEST_CASE("Engine.Flicker.ACandleIsSmoothAndAFailingLampCutsOut", "[engine][animation]") {
    CHECK(LargestJump(FlickerStyle::Candle(), 1U, true) < 0.08f);
    CHECK(LargestJump(FlickerStyle::DeadFluorescent(), 1U, true) > 0.5f);
}

TEST_CASE("Engine.Flicker.WithoutFlashesNothingCutsOutOrMovesFast", "[engine][animation]") {
    for (unsigned seed = 0; seed < 5; ++seed) {
        CHECK(LargestJump(FlickerStyle::DeadFluorescent(), seed, false) < 0.002f);
        CHECK(LargestJump(FlickerStyle::Candle(), seed, false) < 0.02f);
    }
    // A failing lamp burns steadily, near full; a candle still sways.
    float lowest = 1.0f;
    float highest = 0.0f;
    float lampLowest = 1.0f;
    for (int i = 0; i < Samples; ++i) {
        const auto seconds = static_cast<double>(i) * Step;
        const float candle = Flicker(seconds, FlickerStyle::Candle(), 1U, false);
        lowest = std::min(lowest, candle);
        highest = std::max(highest, candle);
        lampLowest = std::min(lampLowest, Flicker(seconds, FlickerStyle::DeadFluorescent(), 1U, false));
    }
    CHECK(highest - lowest > 0.1f);
    CHECK(lampLowest > 0.95f);
}

TEST_CASE("Engine.Flicker.BadValuesDoNoHarm", "[engine][animation]") {
    const FlickerStyle broken{.depth = 0.5f, .speed = -2.0f, .stutterShare = 1.0f, .stutterEvery = 0.0f};
    const float value = Flicker(3.0, broken, 1U, true);
    CHECK(value >= 0.0f);
    CHECK(value <= 1.0f);
    CHECK(Flicker(std::numeric_limits<double>::infinity(), FlickerStyle::Candle(), 1U, true) == 1.0f);
    CHECK(Flicker(std::nan(""), FlickerStyle::Candle(), 1U, true) == 1.0f);
    // Hours in, it still moves.
    CHECK(Flicker(36000.0, FlickerStyle::Candle(), 1U, true) != Flicker(36000.1, FlickerStyle::Candle(), 1U, true));
}
