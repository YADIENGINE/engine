#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Simulation/StateHash.h"
#include "Tests/Simulation/ToySimulation.h"

namespace {
InputTape Played(const SimulationDefinition& definition, const SimulationStart& start) {
    Scenario scenario(definition, start);
    scenario.Hold("Right");
    scenario.Run(40);
    scenario.Release("Right");
    scenario.Tap("Fire", 1);
    scenario.Run(60);
    scenario.Command("bonus", 2);
    scenario.Run(20);
    return scenario.Tape();
}
} // namespace

TEST_CASE("Engine.Simulation.Verify.MatchesAnUnchangedRun", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    const auto tape = Played(definition, {.seed = 3, .players = 2});
    const auto result = VerifyTape(definition, tape);
    CHECK_FALSE(result.mismatch);
    CHECK(result.steps == 120);
    CHECK(result.sameVersions);
    CHECK(tape.hashes.size() == 121); // Every step, plus the start
    CHECK(FormatStateHash(result.hash) == FormatStateHash(tape.hashes.back()));
}
TEST_CASE("Engine.Simulation.Verify.NamesTheExactStepAtEveryStepHashes", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    auto tape = Played(definition, {.seed = 3, .players = 2});
    // An input the recording did not have, at step 70: the state after 71 steps is the first that differs.
    PlayerInput extra;
    extra.SetHeld(ToyButton::Left, true);
    InputTape::Change change;
    change.at = 70;
    change.player = 0;
    change.input = extra;
    const auto position = std::ranges::find_if(tape.changes, [](const auto& item) { return item.at > 70; });
    tape.changes.insert(position, change);
    const auto result = VerifyTape(definition, tape);
    REQUIRE(result.mismatch);
    CHECK(result.mismatch->firstMismatch == 71);
    REQUIRE(result.mismatch->lastMatch);
    CHECK(*result.mismatch->lastMatch == 70);
}
TEST_CASE("Engine.Simulation.Verify.GivesARangeAtSparseHashes", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    const SimulationStart start{.seed = 3, .players = 1};
    ToySimulation simulation(start);
    SimulationRunner runner(simulation, definition.settings, start);
    runner.SetHashEvery(10);
    PlayerInput input;
    input.SetHeld(ToyButton::Right, true);
    runner.Feed(std::array{input});
    runner.RunTo(100);
    auto tape = runner.Tape();
    tape.changes.push_back({55, 0, PlayerInput{}}); // Stops moving at step 55, unlike the recording
    const auto result = VerifyTape(definition, tape);
    REQUIRE(result.mismatch);
    CHECK(result.mismatch->firstMismatch == 60);
    CHECK(*result.mismatch->lastMatch == 50);
}
TEST_CASE("Engine.Simulation.Verify.ADifferentStartDiffersAtOnce", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    auto tape = Played(definition, {.seed = 3, .players = 2});
    tape.start.seed = 4;
    const auto result = VerifyTape(definition, tape);
    REQUIRE(result.mismatch);
    CHECK(result.mismatch->firstMismatch == 0);
    CHECK_FALSE(result.mismatch->lastMatch);
}
TEST_CASE("Engine.Simulation.Verify.RefusesATapeThatCannotPlay", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    auto tape = Played(definition, {.seed = 3, .players = 2});
    tape.simulation = "engine.other";
    CHECK_THROWS(VerifyTape(definition, tape));
}
TEST_CASE("Engine.Simulation.Diverge.FindsNothingInADeterministicRun", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    CHECK_FALSE(FindDivergence(definition, Played(definition, {.seed = 3, .players = 2})));
}
TEST_CASE("Engine.Simulation.Diverge.NamesTheStepAndFieldOfNondeterminism", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    SimulationStart start{.seed = 3, .players = 1};
    start.parameters["flaky"] = true;
    Scenario scenario(definition, start);
    scenario.Run(80);
    const auto divergence = FindDivergence(definition, scenario.Tape());
    REQUIRE(divergence);
    CHECK(divergence->step == 51); // The flaky read is in step 50, whose result is the state after 51 steps
    REQUIRE(divergence->fields.size() == 1);
    CHECK(divergence->fields[0].path == "score");
}
TEST_CASE("Engine.Simulation.StateAt.ReadsAnyStep", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    const auto tape = Played(definition, {.seed = 3, .players = 2});
    const auto state = StateAt(definition, tape, 40);
    CHECK(state.step == 40);
    CHECK(state.state.At("steps").AsUInt64() == 40);
    CHECK(state.hash == tape.hashes[40]);
    CHECK(StateAt(definition, tape, 9999).step == 120);
}
