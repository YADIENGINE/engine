#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include "Utilities/Math/NoiseField.h"

using Catch::Approx;

TEST_CASE("Engine.NoiseField.TheSameSettingsGiveTheSameValue", "[engine][math]") {
    const NoiseSettings settings{.fractal = NoiseFractal::FBm, .seed = 3U};
    CHECK(Noise(settings, Float2{1.25f, -4.5f}) == Noise(settings, Float2{1.25f, -4.5f}));
    CHECK(Noise(NoiseSettings{.seed = 3U}, 1.25f) == Noise(NoiseSettings{.seed = 3U}, 1.25f));
    CHECK(Noise(NoiseSettings{.seed = 3U}, 1.25f) != Noise(NoiseSettings{.seed = 4U}, 1.25f));
    CHECK(Warp(settings, 2.0f, {3.0f, 5.0f}).x == Warp(settings, 2.0f, {3.0f, 5.0f}).x);
}

TEST_CASE("Engine.NoiseField.EveryKindStaysInRangeAndVaries", "[engine][math]") {
    for (const NoiseKind kind :
         {NoiseKind::Simplex, NoiseKind::SimplexSmooth, NoiseKind::Cellular, NoiseKind::Perlin, NoiseKind::Value})
        for (const NoiseFractal fractal :
             {NoiseFractal::None, NoiseFractal::FBm, NoiseFractal::Ridged, NoiseFractal::PingPong}) {
            const NoiseSettings settings{.kind = kind, .fractal = fractal, .seed = 11U};
            float low = 1.0f;
            float high = -1.0f;
            for (int i = 0; i < 400; ++i) {
                const float value = Noise(settings, Float2{static_cast<float>(i) * 0.37f, static_cast<float>(i % 17)});
                low = std::min(low, value);
                high = std::max(high, value);
            }
            CHECK(low >= -1.0f);
            CHECK(high <= 1.0f);
            CHECK(high - low > 0.3f);
        }
}

TEST_CASE("Engine.NoiseField.ALineWandersWithoutJumpsOrStops", "[engine][math]") {
    const NoiseSettings settings{.seed = 3U};
    float largestStep = 0.0f;
    for (int i = 0; i < 4000; ++i) {
        const float x = static_cast<float>(i) * 0.001f;
        largestStep = std::max(largestStep, std::abs(Noise(settings, x + 0.001f) - Noise(settings, x)));
    }
    CHECK(largestStep < 0.01f); // No jumps
    // Value noise stood still at every whole number, so a shake slowed to a stop once per unit.
    int still = 0;
    for (int whole = 1; whole <= 20; ++whole) {
        const auto at = static_cast<float>(whole);
        if (std::abs(Noise(settings, at + 0.01f) - Noise(settings, at - 0.01f)) < 1e-4f) ++still;
    }
    CHECK(still < 3);
}

TEST_CASE("Engine.NoiseField.FrequencyScalesTheField", "[engine][math]") {
    const NoiseSettings coarse{.frequency = 0.5f, .seed = 9U};
    const NoiseSettings fine{.frequency = 1.0f, .seed = 9U};
    CHECK(Noise(coarse, Float2{4.0f, 6.0f}) == Approx(Noise(fine, Float2{2.0f, 3.0f})).margin(1e-5f));
}

TEST_CASE("Engine.NoiseField.WarpPushesByAboutItsAmplitude", "[engine][math]") {
    for (const NoiseKind kind : {NoiseKind::Simplex, NoiseKind::Value}) {
        const NoiseSettings settings{.kind = kind, .seed = 5U};
        float farthest = 0.0f;
        for (int i = 0; i < 20000; ++i) {
            const Float2 point{static_cast<float>(i % 200) * 0.137f, static_cast<float>(i / 200) * 0.291f};
            farthest = std::max(farthest, Length(Warp(settings, 2.0f, point) - point));
        }
        CHECK(farthest > 1.4f);
        CHECK(farthest < 2.2f);
        CHECK(Warp(settings, 0.0f, {1.0f, 2.0f}).x == Approx(1.0f));
    }
}

TEST_CASE("Engine.NoiseField.BadInputDoesNoHarm", "[engine][math]") {
    const NoiseSettings broken{.frequency = std::nanf(""), .octaves = -4, .seed = 1U};
    CHECK(std::isfinite(Noise(broken, Float2{std::nanf(""), INFINITY})));
    CHECK(std::isfinite(Noise(NoiseSettings{}, std::nanf(""))));
}

TEST_CASE("Engine.NoiseField.EveryKindChangesAsOftenAtTheSameFrequency", "[engine][math]") {
    // Turning points per unit along lines through the field: 'frequency' means the same for every kind.
    for (const NoiseKind kind :
         {NoiseKind::Simplex, NoiseKind::SimplexSmooth, NoiseKind::Cellular, NoiseKind::Perlin, NoiseKind::Value}) {
        int turns = 0;
        for (int line = 0; line < 20; ++line) {
            const NoiseSettings settings{.kind = kind, .seed = static_cast<unsigned>(line)};
            const float y = static_cast<float>(line) * 3.7f;
            float previous = Noise(settings, Float2{0.0f, y});
            float slope = 0.0f;
            for (int i = 1; i <= 10000; ++i) {
                const float value = Noise(settings, Float2{static_cast<float>(i) * 0.01f, y});
                const float step = value - previous;
                if (slope * step < 0.0f) ++turns;
                if (step != 0.0f) slope = step;
                previous = value;
            }
        }
        CHECK(static_cast<float>(turns) / (20.0f * 100.0f) == Approx(0.67f).epsilon(0.15));
    }
}
