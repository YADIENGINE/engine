#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "Graphics/Anim/Easing.h"
#include "Utilities/MathUtils.h"

using Catch::Approx;

TEST_CASE("Engine.Easing.CurvesClampAndHitEndpoints", "[engine][math]") {
    for (const auto curve : {EaseInCubic, EaseOutCubic, EaseInOutCubic, EaseOutBack}) {
        CHECK(curve(0.0f) == Approx(0.0f).margin(1e-6));
        CHECK(curve(1.0f) == Approx(1.0f));
        CHECK(curve(-3.0f) == Approx(curve(0.0f)).margin(1e-6));
        CHECK(curve(7.0f) == Approx(curve(1.0f)));
    }
    CHECK(Clamp01(-0.5f) == 0.0f);
    CHECK(Clamp01(0.25f) == 0.25f);
    CHECK(Clamp01(2.0f) == 1.0f);
    CHECK(EaseInCubic(0.5f) == Approx(0.125f));
    CHECK(EaseOutCubic(0.5f) == Approx(0.875f));
    CHECK(EaseInOutCubic(0.25f) == Approx(0.0625f));
    CHECK(EaseInOutCubic(0.5f) == Approx(0.5f));
    CHECK(EaseInOutCubic(0.75f) == Approx(0.9375f));
    CHECK(EaseOutBack(0.5f) > 1.0f); // Hedefi aşıp geri gelir
}

TEST_CASE("Engine.Easing.CountDownAndBlink", "[engine][math]") {
    CHECK(CountDown(1.0f, 0.25f) == Approx(0.75f));
    CHECK(CountDown(0.2f, 0.25f) == 0.0f);
    CHECK_FALSE(BlinkOff(2.0f, 1.0f, 0.5f));
    CHECK(BlinkOff(0.1f, 1.0f, 0.5f));
    CHECK_FALSE(BlinkOff(0.4f, 1.0f, 0.5f));
}

TEST_CASE("Engine.Easing.StaggeredStartsEachItemOneStepLater", "[engine][math]") {
    static_assert(Staggered(0.5f, 0.5f, 0.0f, 0.25f, 1.0f) == 0.0f);
    static_assert(Staggered(1.0f, 0.5f, 0.0f, 0.25f, 1.0f) == 0.5f);
    static_assert(Staggered(1.0f, 0.5f, 2.0f, 0.25f, 1.0f) == 0.0f); // 2. öğe 1.0'da başlar
    static_assert(Staggered(9.0f, 0.5f, 2.0f, 0.25f, 1.0f) == 1.0f);
    CHECK(Staggered(0.75f, 0.0f, 2.0f, 0.25f, 1.0f) == Approx(0.25f));
}

TEST_CASE("Engine.Math.MixFractAndRandomRange", "[engine][math]") {
    CHECK(Mix(2.0f, 6.0f, 0.25f) == Approx(3.0f));
    CHECK(Mix(2.0f, 6.0f, 1.5f) == Approx(8.0f)); // Kırpılmaz
    CHECK(Fract(3.75f) == Approx(0.75f));
    CHECK(Fract(-0.25f) == Approx(0.75f));
    for (const unsigned seed : {0u, 1u, 7u, 12345u}) {
        CAPTURE(seed);
        CHECK(Random01(seed) >= 0.0f);
        CHECK(Random01(seed) < 1.0f);
    }
}
