#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Tests/Simulation/ToyCollision.h"

namespace {
InputTape Played(const SimulationDefinition& definition, std::uint64_t seed) {
    Scenario scenario(definition, {.seed = seed, .players = 1});
    for (int round = 0; round < 5; ++round) {
        scenario.Tap("Fire");
        scenario.Run(8);
        scenario.Hold(round % 2 == 0 ? "Right" : "Left");
        scenario.Run(40);
        scenario.Release(round % 2 == 0 ? "Right" : "Left");
    }
    return scenario.Tape();
}
} // namespace

TEST_CASE("Engine.Physics.Replay.AnEqualRunGivesEqualHashes", "[engine][physics][simulation]") {
    const auto definition = ToyCollisionDefinition();
    const auto tape = Played(definition, 9);
    const auto result = VerifyTape(definition, tape);
    CHECK_FALSE(result.mismatch);
    CHECK(result.hashes == tape.steps + 1U);
    CHECK_FALSE(FindDivergence(definition, tape));
}
TEST_CASE("Engine.Physics.Replay.AChangedStartIsNoticed", "[engine][physics][simulation]") {
    const auto definition = ToyCollisionDefinition();
    auto tape = Played(definition, 9);
    tape.start.seed = 10;
    const auto result = VerifyTape(definition, tape);
    REQUIRE(result.mismatch);
    CHECK(result.mismatch->firstMismatch == 0);
}
TEST_CASE("Engine.Physics.Replay.AFastBulletHitsAThinEnemy", "[engine][physics][simulation]") {
    const auto definition = ToyCollisionDefinition();
    Scenario scenario(definition, {.seed = 9, .players = 1});
    const auto score = [&] { return dynamic_cast<const ToyCollisionSimulation&>(scenario.Sim()).Score(); };
    const auto walls = [&] { return dynamic_cast<const ToyCollisionSimulation&>(scenario.Sim()).WallHits(); };
    scenario.Tap("Fire");
    scenario.Run(10);
    CHECK(score() == 1); // 100 units per step against an enemy a few units thick
    CHECK(walls() == 0);
    scenario.Tap("Fire"); // The enemy is gone, so this one reaches the wall
    scenario.Run(10);
    CHECK(score() == 1);
    CHECK(walls() == 1);
}
