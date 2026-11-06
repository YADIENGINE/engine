#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "Simulation/ReplayCheck.h"
#include "Simulation/Scenario.h"
#include "Simulation/StateHash.h"
#include "Tests/Simulation/ToyProcedural.h"

// Noise, scattering, a cave and a loot table inside a simulation (D49): the same seed gives the same hash at every
// step, and a replay of the tape finds no difference.
namespace {
InputTape Played(const SimulationDefinition& definition, std::uint64_t seed) {
    Scenario scenario(definition, {.seed = seed, .players = 1});
    scenario.Run(300);
    return scenario.Tape();
}
const ToyProceduralRun& RunOf(const Scenario& scenario) {
    return dynamic_cast<const ToyProceduralSimulation&>(scenario.Sim()).Run();
}
} // namespace

TEST_CASE("Engine.Procedural.Replay.TheSameSeedGivesTheSameHashes", "[engine][simulation][procedural]") {
    const auto definition = ToyProceduralDefinition();
    const auto first = Played(definition, 5);
    const auto second = Played(definition, 5);
    REQUIRE(first.hashes.size() == 301);
    CHECK(first.hashes == second.hashes);
    const auto result = VerifyTape(definition, first);
    CHECK_FALSE(result.mismatch);
    CHECK(result.hashes == 301);
    CHECK_FALSE(FindDivergence(definition, first));
}
TEST_CASE("Engine.Procedural.Replay.AnotherSeedGivesAnotherLevel", "[engine][simulation][procedural]") {
    const auto definition = ToyProceduralDefinition();
    Scenario first(definition, {.seed = 5, .players = 1});
    Scenario second(definition, {.seed = 6, .players = 1});
    CHECK_FALSE(RunOf(first).map == RunOf(second).map);
    CHECK(first.Hash() != second.Hash());
}
TEST_CASE("Engine.Procedural.Replay.EveryPartIsInTheState", "[engine][simulation][procedural]") {
    const auto definition = ToyProceduralDefinition();
    Scenario scenario(definition, {.seed = 5, .players = 1});
    scenario.Run(100);
    const auto& run = RunOf(scenario);
    CHECK_FALSE(run.gems.empty());
    CHECK(run.noise != 0.0);
    int drops = 0;
    for (const int count : run.drops)
        drops += count;
    CHECK(drops == 20); // Every fifth step
    const auto state = scenario.State();
    for (const char* name :
         {"map.size.x", "map.cells[0]", "gems[0].x", "wisp.x", "loot.sinceDrawn[3]", "loot.last", "rng.state", "noise"})
        CHECK(state.Members().contains(name));
}
