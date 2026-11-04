#include <catch2/catch_test_macros.hpp>
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Simulation/StateHash.h"
#include "Tests/Simulation/ToyAnimation.h"

TEST_CASE("Engine.Animation.AnimatorsAndTweensReplayExactly", "[engine][animation][simulation]") {
    const auto definition = ToyAnimationDefinition();
    Scenario scenario(definition, {.seed = 3, .players = 1});
    scenario.Run(2000);
    const auto& simulation = dynamic_cast<const ToyAnimationSimulation&>(scenario.Sim());
    // The run has to reach what it claims to test.
    CHECK(simulation.Shots() > 0);
    CHECK(simulation.Interrupted() > 0);
    const auto replay = VerifyTape(definition, scenario.Tape());
    CHECK_FALSE(replay.mismatch);
    CHECK(replay.hashes == scenario.Tape().steps + 1U);
    CHECK_FALSE(FindDivergence(definition, scenario.Tape()));
}
TEST_CASE("Engine.Animation.AChangedStartIsNoticed", "[engine][animation][simulation]") {
    const auto definition = ToyAnimationDefinition();
    Scenario scenario(definition, {.seed = 3, .players = 1});
    scenario.Run(200);
    auto tape = scenario.Tape();
    tape.start.seed = 4;
    CHECK(VerifyTape(definition, tape).mismatch);
}
TEST_CASE("Engine.Animation.AColorReachesTheHash", "[engine][animation][simulation]") {
    struct Colored final : Simulation {
        Color4 color;
        void Step(const SimulationStep&) override {}
        void State(StateVisitor& state) const override { state.Field("color", color); }
    };
    Colored first;
    Colored second;
    CHECK(HashState(first) == HashState(second));
    second.color.b = 1;
    CHECK(HashState(first) != HashState(second));
}
