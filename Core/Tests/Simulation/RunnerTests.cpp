#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <vector>
#include "Simulation/SimulationRunner.h"
#include "Simulation/StateHash.h"
#include "Tests/Simulation/ToySimulation.h"

namespace {
SimulationStart OnePlayer() { return {.seed = 5, .players = 1}; }
PlayerInput Press(ToyButton button) {
    PlayerInput input;
    input.SetPressed(button);
    input.SetHeld(button, true);
    return input;
}
PlayerInput Hold(ToyButton button) {
    PlayerInput input;
    input.SetHeld(button, true);
    return input;
}
void FeedOne(SimulationRunner& runner, const PlayerInput& input) { runner.Feed(std::array{input}); }
constexpr float Frame60 = 1.0f / 60.0f;
} // namespace

TEST_CASE("Engine.Simulation.Runner.FloatFramesGiveExactSteps", "[engine][simulation]") {
    for (const auto [rate, frames, steps] :
         std::array<std::array<int, 3>, 3>{{{60, 6000, 6000}, {120, 6000, 12000}, {30, 6000, 3000}}}) {
        auto definition = ToyDefinition();
        definition.settings.stepsPerSecond = rate;
        ToySimulation simulation(OnePlayer());
        SimulationRunner runner(simulation, definition.settings, OnePlayer());
        for (int i = 0; i < frames; ++i)
            (void)runner.Advance(Frame60);
        CHECK(runner.StepIndex() == static_cast<std::uint64_t>(steps));
    }
}
TEST_CASE("Engine.Simulation.Runner.ExactStepDurationRunsOneStep", "[engine][simulation]") {
    for (const int rate : {1, 7, 30, 60, 90, 120, 144, 240}) {
        auto definition = ToyDefinition();
        definition.settings.stepsPerSecond = rate;
        ToySimulation simulation(OnePlayer());
        SimulationRunner runner(simulation, definition.settings, OnePlayer());
        for (int i = 1; i <= 2000; ++i) {
            CHECK(runner.Advance(1.0f / static_cast<float>(rate)) == 1);
            REQUIRE(runner.StepIndex() == static_cast<std::uint64_t>(i));
        }
    }
}
TEST_CASE("Engine.Simulation.Runner.PressInAFrameWithoutAStepIsKept", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    FeedOne(runner, Press(ToyButton::Fire));
    CHECK(runner.Advance(0.001) == 0); // No step this frame
    FeedOne(runner, PlayerInput{});    // Released before any step ran
    CHECK(runner.Advance(0.001) == 0);
    CHECK(simulation.Shots(0) == 0);
    CHECK(runner.Advance(Frame60) == 1);
    CHECK(simulation.Shots(0) == 1);
    CHECK(simulation.HeldSteps(0) == 0); // A tap shorter than a step: pressed, never held
    CHECK(runner.Advance(Frame60) == 1);
    CHECK(simulation.Shots(0) == 1);
}
TEST_CASE("Engine.Simulation.Runner.PressInAFrameWithSeveralStepsCountsOnce", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    FeedOne(runner, Press(ToyButton::Fire));
    CHECK(runner.Advance(3.0 / 60.0) == 3);
    CHECK(simulation.Shots(0) == 1);
    CHECK(simulation.HeldSteps(0) == 3);
}
TEST_CASE("Engine.Simulation.Runner.ZeroTimeKeepsAPressForLater", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    FeedOne(runner, Press(ToyButton::Fire));
    for (int i = 0; i < 10; ++i)
        CHECK(runner.Advance(0.0) == 0); // Hitstop frames
    FeedOne(runner, Hold(ToyButton::Fire));
    CHECK(runner.Advance(Frame60) == 1);
    CHECK(simulation.Shots(0) == 1);
}
TEST_CASE("Engine.Simulation.Runner.CancelDropsPressesAndReleasesHeld", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    FeedOne(runner, Hold(ToyButton::Right));
    (void)runner.Advance(Frame60);
    const float moved = simulation.X(0);
    CHECK(moved > 0.0f);
    FeedOne(runner, Press(ToyButton::Fire));
    runner.CancelInput();
    (void)runner.Advance(Frame60);
    CHECK(simulation.Shots(0) == 0);
    CHECK(simulation.X(0) == moved);
}
TEST_CASE("Engine.Simulation.Runner.TooMuchOwedTimeIsDropped", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    CHECK(runner.Advance(10.0) == 15); // A quarter of a second at 60 steps per second
    CHECK(runner.Advance(Frame60) == 1);
}
TEST_CASE("Engine.Simulation.Runner.AlphaIsTheFractionOfAStep", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    FeedOne(runner, Hold(ToyButton::Right));
    (void)runner.Advance(1.0 / 60.0);
    (void)runner.Advance(0.5 / 60.0);
    CHECK(runner.Alpha() == Catch::Approx(0.5f));
    CHECK(simulation.SmoothedX(0, runner.Alpha()) == Catch::Approx(1.0f)); // Halfway from 0 to the first step's 2
    FeedOne(runner, Hold(ToyButton::Right));
    (void)runner.Advance(0.5 / 60.0);
    CHECK(runner.Alpha() == Catch::Approx(0.0f));
    CHECK(simulation.SmoothedX(0, 0.5f) == Catch::Approx(1.5f * 120.0f / 60.0f));
}
TEST_CASE("Engine.Simulation.Runner.ReplayDoesNotDependOnFrameTiming", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    const SimulationStart start{.seed = 9, .players = 2};
    ToySimulation recorded(start);
    SimulationRunner recorder(recorded, definition.settings, start);
    Rng frames(1); // Irregular frame durations, including frames with no step and with several
    for (int frame = 0; frame < 2000; ++frame) {
        std::array<PlayerInput, 2> input;
        if (frame % 7 < 3) input[0].SetHeld(ToyButton::Right, true);
        if (frame % 41 == 0) input[1] = Press(ToyButton::Fire);
        input[1].SetAxis(ToyAxis::Aim, static_cast<float>(frame % 13) * 3.3f);
        recorder.Feed(input);
        (void)recorder.Advance(frames.Float(0.0f, 0.05f));
    }
    REQUIRE(recorder.StepIndex() > 1000);
    const auto tape = recorder.Tape();
    const auto final = HashState(recorded);

    for (const double frame : {1.0 / 60.0, 1.0 / 144.0, 3.0 / 60.0, 0.007}) {
        ToySimulation replayed(start);
        SimulationRunner player(replayed, definition.settings, start);
        player.Play(tape, AfterTape::Stop);
        while (!player.Finished())
            (void)player.Advance(frame);
        CHECK(player.StepIndex() == tape.steps);
        CHECK_FALSE(player.Mismatch());
        CHECK(HashState(replayed) == final);
    }
}
TEST_CASE("Engine.Simulation.Runner.StoppedTapeIgnoresInput", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation recorded(OnePlayer());
    SimulationRunner recorder(recorded, definition.settings, OnePlayer());
    recorder.RunTo(10);
    ToySimulation replayed(OnePlayer());
    SimulationRunner player(replayed, definition.settings, OnePlayer());
    player.Play(recorder.Tape(), AfterTape::Stop);
    FeedOne(player, Press(ToyButton::Fire));
    player.Command("bonus", 5);
    player.RunTo(100);
    CHECK(player.StepIndex() == 10);
    CHECK(player.Finished());
    CHECK_FALSE(player.RunStep());
    CHECK(replayed.Shots(0) == 0);
    CHECK(replayed.Score() == 0);
}
TEST_CASE("Engine.Simulation.Runner.TapeThenLiveExtendsIt", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation recorded(OnePlayer());
    SimulationRunner recorder(recorded, definition.settings, OnePlayer());
    FeedOne(recorder, Hold(ToyButton::Right));
    recorder.RunTo(10);
    ToySimulation resumed(OnePlayer());
    SimulationRunner player(resumed, definition.settings, OnePlayer());
    player.Play(recorder.Tape(), AfterTape::Live);
    FeedOne(player, Hold(ToyButton::Left)); // Held state alone does not cut the tape
    player.RunTo(5);
    CHECK(player.Replaying());
    CHECK(player.Tape().steps == 10);
    player.RunTo(10);
    CHECK_FALSE(player.Replaying());
    const float atEnd = resumed.X(0);
    CHECK(atEnd == recorded.X(0));
    player.RunTo(11); // Now the live input counts
    CHECK(resumed.X(0) < atEnd);
    CHECK(player.Tape().steps == 11);
}
TEST_CASE("Engine.Simulation.Runner.PressOrCommandDuringATapeCutsIt", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation recorded(OnePlayer());
    SimulationRunner recorder(recorded, definition.settings, OnePlayer());
    FeedOne(recorder, Press(ToyButton::Fire));
    recorder.RunTo(1);
    FeedOne(recorder, PlayerInput{});
    recorder.RunTo(20);
    recorder.Command("bonus", 3);
    recorder.RunTo(30);
    REQUIRE(recorder.Tape().commands.size() == 1);

    SECTION("a press") {
        ToySimulation resumed(OnePlayer());
        SimulationRunner player(resumed, definition.settings, OnePlayer());
        player.Play(recorder.Tape(), AfterTape::Live);
        player.RunTo(5);
        FeedOne(player, Press(ToyButton::Fire));
        CHECK(player.Tape().steps == 5);
        CHECK_FALSE(player.Replaying());
        player.RunTo(6);
        CHECK(resumed.Shots(0) == 2);
        CHECK(player.Tape().commands.empty());
    }
    SECTION("a command") {
        ToySimulation resumed(OnePlayer());
        SimulationRunner player(resumed, definition.settings, OnePlayer());
        player.Play(recorder.Tape(), AfterTape::Live);
        player.RunTo(5);
        const int before = resumed.Score();
        player.Command("bonus", 8);
        CHECK(player.Tape().steps == 5);
        player.RunTo(6);
        CHECK(resumed.Score() == before + 8);
        REQUIRE(player.Tape().commands.size() == 1);
        CHECK(player.Tape().commands[0].at == 5);
    }
}
TEST_CASE("Engine.Simulation.Runner.PlayRejectsAnotherSimulation", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    auto other = definition.settings;
    other.id = "engine.other";
    CHECK_THROWS(runner.Play(InputTape(other, OnePlayer()), AfterTape::Stop));
    other = definition.settings;
    other.stepsPerSecond = 120;
    CHECK_THROWS(runner.Play(InputTape(other, OnePlayer()), AfterTape::Stop));
    other = definition.settings;
    other.buttons.push_back("Jump");
    CHECK_THROWS(runner.Play(InputTape(other, OnePlayer()), AfterTape::Stop));
    (void)runner.RunStep();
    CHECK_THROWS(runner.Play(InputTape(definition.settings, OnePlayer()), AfterTape::Stop));
}
TEST_CASE("Engine.Simulation.Settings.AreValidated", "[engine][simulation]") {
    auto settings = ToyDefinition().settings;
    settings.stepsPerSecond = 241;
    CHECK_THROWS(ValidateSimulation(settings));
    settings.stepsPerSecond = 0;
    CHECK_THROWS(ValidateSimulation(settings));
    settings = ToyDefinition().settings;
    settings.buttons.assign(33, "x");
    CHECK_THROWS(ValidateSimulation(settings));
    CHECK_THROWS(ValidateSimulationStart(ToyDefinition().settings, {.players = 3}));
}
