#include "Animation/Easing.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string_view>
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Tests/Simulation/ToyTimeline.h"

using Catch::Approx;

namespace {
struct NamedCurve {
    std::string_view name;
    float (*curve)(float);
};
constexpr std::array NewCurves{
    NamedCurve{"Linear", EaseLinear},         NamedCurve{"InQuad", EaseInQuad},
    NamedCurve{"OutQuad", EaseOutQuad},       NamedCurve{"InOutQuad", EaseInOutQuad},
    NamedCurve{"InQuart", EaseInQuart},       NamedCurve{"OutQuart", EaseOutQuart},
    NamedCurve{"InOutQuart", EaseInOutQuart}, NamedCurve{"InSine", EaseInSine},
    NamedCurve{"OutSine", EaseOutSine},       NamedCurve{"InOutSine", EaseInOutSine},
    NamedCurve{"InExpo", EaseInExpo},         NamedCurve{"OutExpo", EaseOutExpo},
    NamedCurve{"InBack", EaseInBack},         NamedCurve{"InOutBack", EaseInOutBack},
    NamedCurve{"InElastic", EaseInElastic},   NamedCurve{"OutElastic", EaseOutElastic},
    NamedCurve{"InBounce", EaseInBounce},     NamedCurve{"OutBounce", EaseOutBounce},
};
} // namespace

TEST_CASE("Engine.Easing.EveryCurveEndsExactlyAndClamps", "[engine][animation]") {
    for (const auto& [name, curve] : NewCurves) {
        INFO(name);
        CHECK(curve(0.0f) == 0.0f);
        CHECK(curve(1.0f) == 1.0f);
        CHECK(curve(-3.0f) == 0.0f);
        CHECK(curve(7.0f) == 1.0f);
    }
}
TEST_CASE("Engine.Easing.InAndOutMirrorEachOther", "[engine][animation]") {
    constexpr std::array Pairs{
        std::array{EaseInQuad, EaseOutQuad},   std::array{EaseInCubic, EaseOutCubic},
        std::array{EaseInQuart, EaseOutQuart}, std::array{EaseInSine, EaseOutSine},
        std::array{EaseInExpo, EaseOutExpo},   std::array{EaseInBack, EaseOutBack},
        std::array{EaseInElastic, EaseOutElastic}, std::array{EaseInBounce, EaseOutBounce},
    };
    for (const auto& pair : Pairs)
        for (float t = 0.1f; t < 1.0f; t += 0.1f)
            CHECK(pair[0](t) == Approx(1.0f - pair[1](1.0f - t)).margin(1e-3));
}
TEST_CASE("Engine.Easing.InOutCurvesPassThroughTheMiddle", "[engine][animation]") {
    for (const auto curve : {EaseInOutQuad, EaseInOutCubic, EaseInOutQuart, EaseInOutSine, EaseInOutBack})
        CHECK(curve(0.5f) == Approx(0.5f));
}
TEST_CASE("Engine.Easing.CurvesHaveTheirCharacter", "[engine][animation]") {
    CHECK(EaseInBack(0.2f) < 0.0f); // Pulls back first
    CHECK(EaseInOutBack(0.1f) < 0.0f);
    CHECK(EaseInOutBack(0.9f) > 1.0f); // Overshoots at the end
    CHECK(EaseOutElastic(0.2f) > 1.0f);
    CHECK(EaseInElastic(0.8f) < 0.0f);
    for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
        CHECK(EaseOutBounce(t) >= 0.0f); // Bounces stay on the near side of the target
        CHECK(EaseOutBounce(t) <= 1.0f + 1e-5f);
    }
    CHECK(EaseOutBounce(0.545f) < EaseOutBounce(0.36f)); // Back up after the first landing
    static_assert(EaseInQuad(0.5f) == 0.25f);
    static_assert(EaseOutQuad(0.5f) == 0.75f);
    static_assert(EaseInQuart(0.5f) == 0.0625f);
    static_assert(EaseOutBounce(1.0f) == 1.0f);
    static_assert(EaseInBack(1.0f) == 1.0f);
}
TEST_CASE("Engine.Animation.ARunOnTimelinesReplaysExactly", "[engine][animation][simulation]") {
    const auto definition = ToyTimelineDefinition();
    Scenario scenario(definition, {.seed = 1, .players = 1});
    scenario.Run(700);
    const auto& simulation = dynamic_cast<const ToyTimelineSimulation&>(scenario.Sim());
    CHECK(simulation.Waves() == 2);
    CHECK(simulation.Shots() == 9); // 520 steps of the boss at one shot per 60, the first at its step 30
    const auto replay = VerifyTape(definition, scenario.Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hashes == scenario.Tape().steps + 1U);
    CHECK_FALSE(FindDivergence(definition, scenario.Tape()));
}
TEST_CASE("Engine.Animation.MomentsAppearWhenTheyAreDecided", "[engine][animation][simulation]") {
    Scenario scenario(ToyTimelineDefinition(), {.seed = 1, .players = 1});
    const auto stepOf = [&](std::string_view name) -> std::int64_t {
        for (const auto& moment : scenario.Sim().Moments())
            if (moment.name == name) return static_cast<std::int64_t>(moment.step);
        return -1;
    };
    CHECK(stepOf("boss") == 180);
    CHECK(stepOf("attack.fire") == -1);
    scenario.Run(181);
    CHECK(stepOf("attack.fire") == 210);
}
