#include "Animation/Animator.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using Catch::Approx;

// Outside the anonymous namespace: State reads field names from the type (Utilities/Reflection.h).
struct AnimatorTestPose {
    float arm = 0.0f;
    int frame = 0;
};

namespace {
StepClipSet<AnimatorTestPose> Clips() {
    return {
        {"idle", StepClip<AnimatorTestPose>{}.Track(&AnimatorTestPose::arm, {{0, 0.0f}, {10, 1.0f}}).Moment("tick", 0).Looping()},
        {"walk", StepClip<AnimatorTestPose>{}.Track(&AnimatorTestPose::arm, {{0, 10.0f}, {10, 20.0f}}).Moment("step", 5).Looping()},
        {"attack", StepClip<AnimatorTestPose>{}
                       .Track(&AnimatorTestPose::arm, {{0, 100.0f}, {4, 104.0f}})
                       .Track(&AnimatorTestPose::frame, {{0, 1}, {2, 2}})
                       .Moment("start", 0)
                       .Moment("fire", 2)
                       .Moment("end", 4)},
        {"pose", StepClip<AnimatorTestPose>{}.Track(&AnimatorTestPose::frame, {{0, 9}}).Moment("once", 0)},
    };
}
struct Log {
    std::vector<std::string> names;
    auto Recorder() {
        return [this](std::string_view name) { names.emplace_back(name); };
    }
    std::vector<std::string> Take() { return std::exchange(names, {}); }
};
// Records a run's state, as a simulation's hash and dump would see it.
struct Dump {
    std::string text;
    void Field(std::string_view name, const auto& value) {
        text += std::string(name) + "=";
        if constexpr (std::is_enum_v<std::decay_t<decltype(value)>>)
            text += std::to_string(static_cast<int>(value));
        else if constexpr (std::is_convertible_v<decltype(value), std::string_view>)
            text += std::string(std::string_view(value));
        else
            text += std::to_string(value);
        text += ";";
    }
    template <class Body> void Group(std::string_view name, Body&& body) {
        text += std::string(name) + "{";
        body();
        text += "}";
    }
};
std::string StateOf(const StepAnimator<AnimatorTestPose>& animator) {
    Dump dump;
    animator.State(dump);
    return dump.text;
}
} // namespace

TEST_CASE("Engine.Animator.PlaysItsFirstClipAndLoops", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    CHECK(animator.Clip() == "idle");
    CHECK(animator.Pose().arm == 0.0f);
    for (int i = 0; i < 3; ++i)
        animator.Step();
    CHECK(animator.Clock() == 3);
    CHECK(animator.Pose().arm == Approx(0.3f));
    for (int i = 0; i < 7; ++i)
        CHECK_FALSE(animator.Step()); // A looping clip never ends
    CHECK(animator.Clock() == 0);
    CHECK_FALSE(animator.Finished());
    CHECK(animator.Pose().arm == 0.0f);
}
TEST_CASE("Engine.Animator.RefusesUnknownNamesAndBadFades", "[engine][animation]") {
    CHECK_THROWS(StepAnimator<AnimatorTestPose>(Clips(), "nope"));
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    CHECK_THROWS(animator.Play("nope"));
    CHECK_THROWS(animator.Play("walk", {.then = "nope"}));
    CHECK_THROWS(animator.Play("walk", {.blend = -1}));
    CHECK_THROWS(animator.Play("walk", {.thenBlend = -1}));
    CHECK(animator.Clip() == "idle"); // Refused before anything changed
}
TEST_CASE("Engine.Animator.MeetsEachMomentOncePerPass", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "attack");
    Log log;
    const auto run = [&] {
        animator.Step(log.Recorder());
        return log.Take();
    };
    CHECK(run() == std::vector<std::string>{"start"});
    CHECK(run().empty());
    CHECK(run() == std::vector<std::string>{"fire"});
    CHECK_FALSE(animator.JustFinished());
    CHECK(run() == std::vector<std::string>{"end"}); // A moment at the very end comes with the end
    CHECK(animator.JustFinished());
    CHECK(animator.Finished());
    CHECK(run().empty());
    CHECK_FALSE(animator.JustFinished());
    CHECK(animator.Pose().arm == 104.0f); // Stays on its last pose
}
TEST_CASE("Engine.Animator.ALoopMeetsAMomentEveryRound", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "walk");
    int steps = 0;
    for (int i = 0; i < 30; ++i)
        animator.Step([&](std::string_view) { ++steps; });
    CHECK(steps == 3);
}
TEST_CASE("Engine.Animator.AClipWithNoLengthMeetsItsMomentsOnce", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "pose");
    Log log;
    CHECK(animator.Step(log.Recorder()));
    CHECK(log.Take() == std::vector<std::string>{"once"});
    CHECK_FALSE(animator.Step(log.Recorder()));
    CHECK(log.Take().empty());
    CHECK(animator.Pose().frame == 9);
}
TEST_CASE("Engine.Animator.PlayingTheCurrentClipDoesNothing", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "walk");
    animator.Step();
    animator.Step();
    animator.Play("walk");
    CHECK(animator.Clock() == 2);
    animator.Play("walk", {.restart = true});
    CHECK(animator.Clock() == 0);
}
TEST_CASE("Engine.Animator.ACutChangesAtOnce", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Step();
    animator.Play("walk");
    CHECK_FALSE(animator.Blending());
    CHECK(animator.Pose().arm == 10.0f);
}
TEST_CASE("Engine.Animator.AFadeMixesBothClipsThatKeepRunning", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle"); // arm: step / 10
    animator.Step();
    animator.Play("walk", {.blend = 4}); // arm: 10 + step
    CHECK(animator.Blending());
    CHECK(animator.Pose().arm == Approx(0.1f)); // Nothing of the new clip yet
    animator.Step();
    CHECK(animator.Pose().arm == Approx(0.2f + (11.0f - 0.2f) * 0.25f));
    animator.Step();
    CHECK(animator.Pose().arm == Approx(0.3f + (12.0f - 0.3f) * 0.5f));
    animator.Step();
    animator.Step();
    CHECK(animator.Pose().arm == 14.0f); // Exactly the new clip at the end
    CHECK(animator.Blending());          // Kept for a view drawing up to this step
    animator.Step();
    CHECK_FALSE(animator.Blending());
    CHECK(animator.Pose().arm == 15.0f);
}
TEST_CASE("Engine.Animator.AFadeOutMeetsNoMoments", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "walk");
    for (int i = 0; i < 4; ++i)
        animator.Step();
    animator.Play("idle", {.blend = 5, .restart = false});
    Log log;
    for (int i = 0; i < 3; ++i)
        animator.Step(log.Recorder());
    CHECK(log.Take() == std::vector<std::string>{"tick"}); // Only idle's; walk's "step" at 5 passed while fading out
}
TEST_CASE("Engine.Animator.AnInterruptedFadeContinuesFromWhatShowed", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Play("walk", {.blend = 4});
    animator.Step();
    animator.Step();
    const float showing = animator.Pose().arm;
    animator.Play("attack", {.blend = 2});
    CHECK(animator.Pose().arm == Approx(showing)); // No jump
    animator.Step();
    CHECK(animator.Pose().arm == Approx(showing + (101.0f - showing) * 0.5f));
    animator.Step();
    CHECK(animator.Pose().arm == 102.0f);
    animator.Step();
    CHECK_FALSE(animator.Blending());
}
TEST_CASE("Engine.Animator.ThenFollowsWhenTheClipEnds", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Play("attack", {.then = "walk", .thenBlend = 2});
    Log log;
    for (int i = 0; i < 3; ++i)
        CHECK_FALSE(animator.Step(log.Recorder()));
    CHECK(animator.Clip() == "attack");
    CHECK(animator.Step(log.Recorder())); // Its last step
    CHECK(animator.JustFinished());
    CHECK(animator.Clip() == "walk");
    CHECK(animator.Blending());
    CHECK(animator.Pose().arm == 104.0f);
    CHECK(log.Take() == std::vector<std::string>{"start", "fire", "end"});
    animator.Step();
    CHECK_FALSE(animator.JustFinished());
    CHECK(animator.Pose().arm == Approx(104.0f + (11.0f - 104.0f) * 0.5f));
}
TEST_CASE("Engine.Animator.AnotherPlayDropsThen", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Play("attack", {.then = "walk"});
    animator.Play("pose");
    animator.Step();
    CHECK(animator.Clip() == "pose");
}
TEST_CASE("Engine.Animator.AMomentMayPlayAnotherClip", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "attack");
    Log log;
    for (int i = 0; i < 3; ++i)
        animator.Step([&](std::string_view moment) {
            log.names.emplace_back(moment);
            if (moment == "fire") animator.Play("walk");
        });
    CHECK(animator.Clip() == "walk");
    CHECK(animator.Clock() == 0); // Its own clock is left alone by the step that started it
    CHECK(log.Take() == std::vector<std::string>{"start", "fire"});
}
TEST_CASE("Engine.Animator.ALastMomentMayReplaceThen", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Play("attack", {.then = "walk"});
    for (int i = 0; i < 4; ++i)
        animator.Step([&](std::string_view moment) {
            if (moment == "end") animator.Play("pose");
        });
    CHECK(animator.Clip() == "pose");
}
TEST_CASE("Engine.Animator.BetweenStepsFollowsStepSmoothed", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    CHECK(animator.Pose(0.5f).arm == 0.0f); // Not moved yet: no step behind it
    animator.Step();
    CHECK(animator.Pose(0.0f).arm == 0.0f);
    CHECK(animator.Pose(0.5f).arm == Approx(0.05f));
    CHECK(animator.Pose(1.0f).arm == Approx(0.1f));
    for (int i = 0; i < 9; ++i)
        animator.Step();
    CHECK(animator.Clock() == 0);                    // Just wrapped
    CHECK(animator.Pose(0.5f).arm == Approx(0.95f)); // Still on its way to the end of the round
}
TEST_CASE("Engine.Animator.BetweenStepsAFinishedClipStaysStill", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "attack");
    for (int i = 0; i < 6; ++i)
        animator.Step();
    CHECK(animator.Pose(0.0f).arm == 104.0f);
    CHECK(animator.Pose(0.5f).arm == 104.0f);
}
TEST_CASE("Engine.Animator.BetweenStepsAFadeMovesSmoothly", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "attack");
    for (int i = 0; i < 4; ++i)
        animator.Step();
    animator.Play("pose", {.blend = 2});
    CHECK(animator.Pose(0.5f).frame == 2);
    animator.Step();
    CHECK(animator.Pose(0.0f).arm == 104.0f);
    CHECK(animator.Pose(0.5f).arm == Approx(104.0f * 0.75f));
    animator.Step();
    CHECK(animator.Pose(0.5f).arm == Approx(104.0f * 0.25f)); // The last step of the fade is still a fade
    CHECK(animator.Pose(1.0f).arm == 0.0f);
}
TEST_CASE("Engine.Animator.OnSecondsCrossesWholeRoundsInOneCall", "[engine][animation]") {
    const SecondsClipSet<AnimatorTestPose> clips{
        {"spin", SecondsClip<AnimatorTestPose>{}.Track(&AnimatorTestPose::arm, {{0.0f, 0.0f}, {1.0f, 1.0f}}).Moment("mark", 0.5f).Looping()},
        {"once", SecondsClip<AnimatorTestPose>{}.Track(&AnimatorTestPose::arm, {{0.0f, 5.0f}, {1.0f, 6.0f}}).Moment("end", 1.0f)},
    };
    SecondsAnimator<AnimatorTestPose> animator(clips, "spin");
    int marks = 0;
    animator.Update(2.75f, [&](std::string_view) { ++marks; });
    CHECK(marks == 3);
    CHECK(animator.Pose().arm == Approx(0.75f));
    CHECK_FALSE(animator.Update(0.0f)); // Nothing moves on no time
    CHECK(animator.Clock() == Approx(0.75f));
    CHECK_FALSE(animator.Update(-1.0f));

    animator.Play("once");
    bool ended = false;
    CHECK(animator.Update(5.0f, [&](std::string_view) { ended = true; }));
    CHECK(ended);
    CHECK(animator.Clock() == 1.0f);
    CHECK(animator.Pose().arm == 6.0f);
}
TEST_CASE("Engine.Animator.StateOnlyHoldsClipsAndClocksInNormalPlay", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> animator(Clips(), "idle");
    animator.Step();
    CHECK(StateOf(animator) == "clip=idle;clock=1;moved=1;done=0;justFinished=0;");
    animator.Play("walk", {.blend = 3, .then = "idle", .thenBlend = 1});
    CHECK(StateOf(animator) == "clip=walk;clock=0;moved=0;done=0;justFinished=0;next{clip=idle;blend=1;curve=0;}"
                               "blend{clock=0;length=3;curve=0;moved=0;from{clip=idle;clock=1;moved=1;done=0;}}");
    animator.Play("attack", {.blend = 2}); // Interrupted: the one case with a frozen pose
    CHECK(StateOf(animator).find("frozen{arm=") != std::string::npos);
}
TEST_CASE("Engine.Animator.CopiesPlayOnIndependently", "[engine][animation]") {
    StepAnimator<AnimatorTestPose> first(Clips(), "idle");
    first.Step();
    StepAnimator<AnimatorTestPose> second = first;
    second.Step();
    second.Play("walk");
    CHECK(first.Clip() == "idle");
    CHECK(first.Clock() == 1);
    CHECK(second.Clip() == "walk");
}
