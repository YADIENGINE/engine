#include <catch2/catch_test_macros.hpp>
#include "Simulation/Scenario.h"
#include "Simulation/ScenarioScript.h"
#include "Tests/Simulation/ToySimulation.h"

namespace {
SimulationRegistry ToyRegistry() {
    SimulationRegistry registry;
    RegisterSimulations(registry);
    return registry;
}
} // namespace

TEST_CASE("Engine.Simulation.Scenario.DrivesASimulationByName", "[engine][simulation]") {
    Scenario scenario(ToyDefinition(), {.seed = 1, .players = 2});
    scenario.Hold("Right");
    scenario.Run(60);
    scenario.Release("Right");
    scenario.Run(10);
    scenario.Tap("Fire", 1);
    scenario.Run(5);
    scenario.Command("bonus", 4);
    scenario.SetAxis("Aim", 2.5f);
    scenario.Run();
    const auto& toy = static_cast<const ToySimulation&>(scenario.Sim());
    CHECK(toy.X(0) == toy.SmoothedX(0, 1.0f));
    CHECK(toy.Shots(1) == 1);
    CHECK(toy.HeldSteps(1) == 1); // A tap holds for one step
    CHECK(toy.Score() >= 4);
    const auto state = scenario.State();
    CHECK(state.At("player[0].aim").AsDouble() == 2.5);
    CHECK(state.At("steps").AsUInt64() == 76);
    CHECK(scenario.Step() == 76);
    CHECK(scenario.Hash() == scenario.Tape().hashes.back());
}
TEST_CASE("Engine.Simulation.Scenario.RefusesUnknownNames", "[engine][simulation]") {
    Scenario scenario(ToyDefinition(), {.players = 1});
    CHECK_THROWS(scenario.Hold("Jump"));
    CHECK_THROWS(scenario.SetAxis("Nope", 1.0f));
    CHECK_THROWS(scenario.Tap("Fire", 1));
}
TEST_CASE("Engine.Simulation.Script.PassesAndReads", "[engine][simulation]") {
    const auto script = JsonIO::Parse(R"({
        "simulation": "engine.toy", "seed": 7, "players": 2, "start": {"startScore": 100},
        "script": [
            {"hold": "Right"}, {"run": 60}, {"release": "Right"},
            {"tap": "Fire", "player": 1}, {"run": 1},
            {"expect": {"steps": 61, "player[1].shots": 1, "player[0].x": {"min": 119, "max": 121}}},
            {"read": ["score", "player[0].x"]}
        ]})");
    const auto result = RunScenarioScript(ToyRegistry(), script);
    CHECK(result.passed);
    CHECK(result.report.At("steps").AsUInt64() == 61);
    const auto& reads = result.report.At("reads").Elements();
    REQUIRE(reads.size() == 1);
    CHECK(reads[0].At("step").AsUInt64() == 61);
    CHECK(reads[0].At("values").At("score").AsInt64() >= 100);
    CHECK(result.tape.steps == 61);
}
TEST_CASE("Engine.Simulation.Script.ReportsFailedExpectations", "[engine][simulation]") {
    const auto script = JsonIO::Parse(R"({
        "simulation": "engine.toy",
        "script": [{"run": 5}, {"expect": {"steps": 6, "score": 0, "enemyX": {"max": -1}}}]})");
    const auto result = RunScenarioScript(ToyRegistry(), script);
    CHECK_FALSE(result.passed);
    const auto& failures = result.report.At("failures").Elements();
    REQUIRE(failures.size() == 2);
    CHECK(failures[0].At("field").AsString() == "enemyX");
    CHECK(failures[1].At("field").AsString() == "steps");
    CHECK(failures[1].At("actual").AsUInt64() == 5);
}
TEST_CASE("Engine.Simulation.Script.RunsUntilAConditionOrItsLimit", "[engine][simulation]") {
    auto result = RunScenarioScript(ToyRegistry(), JsonIO::Parse(R"({
        "simulation": "engine.toy",
        "script": [{"until": {"steps": 25}}]})"));
    CHECK(result.passed);
    CHECK(result.report.At("steps").AsUInt64() == 25);
    result = RunScenarioScript(ToyRegistry(), JsonIO::Parse(R"({
        "simulation": "engine.toy",
        "script": [{"until": {"steps": 25}, "limit": 10}]})"));
    CHECK_FALSE(result.passed);
    CHECK(result.report.At("steps").AsUInt64() == 10);
}
TEST_CASE("Engine.Simulation.Script.RefusesWhatCannotRun", "[engine][simulation]") {
    const auto registry = ToyRegistry();
    CHECK_THROWS(RunScenarioScript(registry, JsonIO::Parse(R"({"simulation": "nope", "script": []})")));
    CHECK_THROWS(RunScenarioScript(registry, JsonIO::Parse(R"({"simulation": "engine.toy", "script": [{"fly": 1}]})")));
    CHECK_THROWS(RunScenarioScript(
        registry, JsonIO::Parse(R"({"simulation": "engine.toy", "script": [{"expect": {"missing": 1}}]})")));
    CHECK_THROWS(
        RunScenarioScript(registry, JsonIO::Parse(R"({"simulation": "engine.toy", "players": 9, "script": []})")));
}
