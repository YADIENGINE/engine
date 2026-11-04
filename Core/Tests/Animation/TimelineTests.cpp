#include "Animation/Timeline.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <vector>
#include "Simulation/TimelineMoments.h"

using Catch::Approx;

namespace {
const StepTimeline& Attack() {
    static const StepTimeline attack{{"windup", 0}, {"fire", 30}, {"recover", 40}, {"cycle", 60}};
    return attack;
}
std::vector<std::string> Crossed(const StepTimeline& timeline, int before, int after) {
    std::vector<std::string> names;
    timeline.ForEachCrossed(before, after, [&](std::string_view name) { names.emplace_back(name); });
    return names;
}
} // namespace

TEST_CASE("Engine.Timeline.RefusesInvalidData", "[engine][animation]") {
    using Moments = std::vector<TimelineMoment<int>>;
    CHECK_THROWS(StepTimeline(Moments{}));
    CHECK_THROWS(StepTimeline{{"", 0}});
    CHECK_THROWS(StepTimeline{{"a", -1}});
    CHECK_THROWS(StepTimeline{{"a", 10}, {"b", 5}});
    CHECK_THROWS(StepTimeline{{"a", 0}, {"a", 5}});
    CHECK_THROWS(SecondsTimeline{{"a", std::numeric_limits<float>::quiet_NaN()}});
    CHECK_THROWS(SecondsTimeline{{"a", std::numeric_limits<float>::infinity()}});
    CHECK_NOTHROW(StepTimeline{{"a", 5}, {"b", 5}});
}
TEST_CASE("Engine.Timeline.UnknownNamesThrow", "[engine][animation]") {
    CHECK_THROWS(Attack().PositionOf("nope"));
    CHECK_THROWS(Attack().Progress("windup", "nope", 3));
    CHECK_THROWS(Attack().Progress("fire", "windup", 3)); // Backwards is a mistake, not an empty range
    CHECK(Attack().Contains("fire"));
    CHECK_FALSE(Attack().Contains("nope"));
    CHECK(Attack().PositionOf("recover") == 40);
    CHECK(Attack().Length() == 60);
}
TEST_CASE("Engine.Timeline.AClockMeetsEachMomentOnce", "[engine][animation]") {
    std::vector<std::string> met;
    for (int age = 0; age < 100; ++age) {
        const auto now = Crossed(Attack(), age, age + 1);
        met.insert(met.end(), now.begin(), now.end());
        // A step that starts at a moment's position is the one that meets it.
        if (age == 0) CHECK(now == std::vector<std::string>{"windup"});
        if (age == 30) CHECK(now == std::vector<std::string>{"fire"});
        if (age == 29 || age == 31) CHECK(now.empty());
    }
    CHECK(met == std::vector<std::string>{"windup", "fire", "recover", "cycle"});
}
TEST_CASE("Engine.Timeline.ABigJumpMeetsEverythingInOrder", "[engine][animation]") {
    CHECK(Crossed(Attack(), 0, 100) == std::vector<std::string>{"windup", "fire", "recover", "cycle"});
    CHECK(Crossed(Attack(), 30, 41) == std::vector<std::string>{"fire", "recover"});
    CHECK(Crossed(Attack(), 31, 40).empty());
    CHECK(Crossed(Attack(), 50, 10).empty()); // Going back meets nothing
    CHECK(Crossed(Attack(), 30, 30).empty()); // Standing still neither
    const StepTimeline together{{"second", 5}, {"first", 5}};
    CHECK(Crossed(together, 5, 6) == std::vector<std::string>{"second", "first"}); // As declared
}
TEST_CASE("Engine.Timeline.PhaseIsTheLatestMomentReached", "[engine][animation]") {
    const StepTimeline late{{"start", 10}, {"end", 20}};
    CHECK(late.PhaseAt(9).empty());
    CHECK(late.PhaseAt(10) == "start");
    CHECK(late.PhaseAt(19) == "start");
    CHECK(late.PhaseAt(20) == "end");
    CHECK(late.PhaseAt(1000) == "end");
    const StepTimeline together{{"a", 0}, {"b", 5}, {"c", 5}};
    CHECK(together.PhaseAt(5) == "c");
    CHECK_FALSE(late.Finished(19));
    CHECK(late.Finished(20));
}
TEST_CASE("Engine.Timeline.ProgressRunsBetweenTwoMoments", "[engine][animation]") {
    CHECK(Attack().Progress("windup", "fire", -5) == 0.0f);
    CHECK(Attack().Progress("windup", "fire", 0) == 0.0f);
    CHECK(Attack().Progress("windup", "fire", 15) == 0.5f);
    CHECK(Attack().Progress("windup", "fire", 30) == 1.0f);
    CHECK(Attack().Progress("windup", "fire", 99) == 1.0f);
    CHECK(Attack().Progress("recover", "cycle", 50) == 0.5f);
    const StepTimeline together{{"a", 5}, {"b", 5}};
    CHECK(together.Progress("a", "b", 4) == 0.0f); // No length: it jumps
    CHECK(together.Progress("a", "b", 5) == 1.0f);
    const SecondsTimeline seconds{{"in", 0.5f}, {"out", 1.5f}};
    CHECK(seconds.Progress("in", "out", 1.0f) == Approx(0.5f));
    CHECK(seconds.Progress("in", "out", 0.25f) == 0.0f);
}
TEST_CASE("Engine.Timeline.BetweenStepsFollowsStepSmoothed", "[engine][animation]") {
    // After the step that took the clock to 15, the view runs from step 14 at alpha 0 to step 15 at alpha 1.
    CHECK(Attack().Progress("windup", "fire", 15, 0.0f) == Attack().Progress("windup", "fire", 14));
    CHECK(Attack().Progress("windup", "fire", 15, 1.0f) == Attack().Progress("windup", "fire", 15));
    CHECK(Attack().Progress("windup", "fire", 15, 0.5f) == Approx(14.5f / 30.0f));
    CHECK(Attack().Progress("windup", "fire", 0, 0.5f) == 0.0f); // Nothing has run yet
    CHECK(Attack().Progress("windup", "fire", 31, 0.5f) == 1.0f);
}
TEST_CASE("Engine.Timeline.WrapRepeatsACycle", "[engine][animation]") {
    CHECK(Attack().Wrap(0) == 0);
    CHECK(Attack().Wrap(59) == 59);
    CHECK(Attack().Wrap(60) == 0);
    CHECK(Attack().Wrap(125) == 5);
    CHECK(Attack().Wrap(-1) == 59);
    int fired = 0;
    int cycleEnds = 0;
    for (int age = 0; age < 180; ++age) {
        const int cycle = Attack().Wrap(age);
        Attack().ForEachCrossed(cycle, cycle + 1, [&](std::string_view name) {
            fired += name == "fire" ? 1 : 0;
            cycleEnds += name == "cycle" ? 1 : 0;
        });
    }
    CHECK(fired == 3);
    CHECK(cycleEnds == 0); // The last moment is the same instant as position 0
    const StepTimeline point{{"only", 0}};
    CHECK(point.Wrap(50) == 0); // No length, nothing to divide by
    const SecondsTimeline seconds{{"a", 0.0f}, {"b", 2.0f}};
    CHECK(seconds.Wrap(5.0f) == Approx(1.0f));
    CHECK(seconds.Wrap(-0.5f) == Approx(1.5f));
}
TEST_CASE("Engine.Timeline.StepsInRoundsSeconds", "[engine][animation]") {
    static_assert(StepsIn(1.5, 60) == 90);
    static_assert(StepsIn(0.0, 60) == 0);
    static_assert(StepsIn(1.0 / 60.0, 60) == 1);
    static_assert(StepsIn(0.01, 60) == 1); // 0.6 of a step
    static_assert(StepsIn(0.5, 120) == 60);
    static_assert(StepsIn(-1.0, 60) == -60);
    CHECK_THROWS(StepTimeline{{"a", StepsIn(-1.0, 60)}});
}
TEST_CASE("Engine.Timeline.AppendMomentsGivesRunSteps", "[engine][animation][simulation]") {
    std::vector<SimulationMoment> moments;
    AppendMoments(moments, Attack());
    AppendMoments(moments, Attack(), 100, "boss.");
    REQUIRE(moments.size() == 8);
    CHECK(moments[1].name == "fire");
    CHECK(moments[1].step == 30);
    CHECK(moments[5].name == "boss.fire");
    CHECK(moments[5].step == 130);
}
