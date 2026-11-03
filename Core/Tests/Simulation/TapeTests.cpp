#include <catch2/catch_test_macros.hpp>
#include <array>
#include "Simulation/InputTape.h"
#include "Simulation/SimulationRunner.h"
#include "Tests/Simulation/ToySimulation.h"
#include "Tests/Support/TempDirectory.h"

namespace {
InputTape RecordedTape() {
    const auto definition = ToyDefinition();
    SimulationStart start{.seed = 18446744073709551557ULL, .players = 2};
    start.parameters["startScore"] = 3;
    ToySimulation simulation(start);
    SimulationRunner runner(simulation, definition.settings, start);
    std::array<PlayerInput, 2> input;
    input[0].SetHeld(ToyButton::Right, true);
    input[1].SetPressed(ToyButton::Fire);
    input[1].SetAxis(ToyAxis::Aim, -12.5f);
    runner.Feed(input);
    runner.RunTo(100);
    input = {};
    runner.Feed(input);
    runner.Command("bonus", 7);
    runner.RunTo(150);
    return runner.Tape();
}
} // namespace

TEST_CASE("Engine.Simulation.Tape.StoresOnlyChanges", "[engine][simulation]") {
    const auto tape = RecordedTape();
    CHECK(tape.steps == 150);
    // Both players at step 0; player 1's press ends at step 1 but held and axes stay, so no change until step 100.
    REQUIRE(tape.changes.size() == 4);
    CHECK(tape.changes[0].at == 0);
    CHECK(tape.changes[1].at == 0);
    CHECK(tape.changes[1].input.Pressed(ToyButton::Fire));
    CHECK(tape.changes[2].at == 100);
    REQUIRE(tape.commands.size() == 1);
    CHECK(tape.commands[0].at == 100);
    CHECK(tape.commands[0].command == SimulationCommand{"bonus", 7});
    CHECK(tape.hashEvery == 60);
    CHECK(tape.hashes.size() == 3); // Steps 0, 60 and 120
}
TEST_CASE("Engine.Simulation.Tape.SurvivesTheFileUnchanged", "[engine][simulation]") {
    const auto tape = RecordedTape();
    TempDirectory directory;
    const auto file = directory.Path() / "run.replay";
    tape.Save(file);
    CHECK(InputTape::Load(file) == tape);
}
TEST_CASE("Engine.Simulation.Tape.UsesActionNames", "[engine][simulation]") {
    const auto document = RecordedTape().ToDocument();
    CHECK(document.At("simulation").AsString() == "engine.toy");
    CHECK(document.At("engine").AsString() == ENGINE_VERSION_FOR_TESTS);
    const auto& input = document.At("input").Elements();
    CHECK(input[0].At("held").Elements()[0].AsString() == "Right");
    CHECK(input[1].At("pressed").Elements()[0].AsString() == "Fire");
    CHECK(input[1].At("axes").At("Aim").AsInt64() == -12800);
}
TEST_CASE("Engine.Simulation.Tape.RefusesBrokenDocuments", "[engine][simulation]") {
    const auto valid = RecordedTape().ToDocument();
    auto broken = valid;
    broken["format"] = 99;
    CHECK_THROWS(InputTape::FromDocument(broken));
    broken = valid;
    broken["hashes"] = "abc";
    CHECK_THROWS(InputTape::FromDocument(broken));
    broken = valid;
    broken["hashEvery"] = 0;
    CHECK_THROWS(InputTape::FromDocument(broken));
    broken = valid;
    broken["input"].Elements()[0]["held"] = JsonIO::Document::Array{"NoSuchButton"};
    CHECK_THROWS(InputTape::FromDocument(broken));
    broken = valid;
    broken["input"].Elements()[0]["at"] = 1000;
    CHECK_THROWS(InputTape::FromDocument(broken));
    broken = valid;
    broken["input"].Elements()[0]["player"] = 5;
    CHECK_THROWS(InputTape::FromDocument(broken));
}
TEST_CASE("Engine.Simulation.Tape.TruncateForgetsTheRest", "[engine][simulation]") {
    auto tape = RecordedTape();
    tape.Truncate(100);
    CHECK(tape.steps == 100);
    CHECK(tape.changes.size() == 2);
    CHECK(tape.commands.empty());
    CHECK(tape.hashes.size() == 2); // Steps 0 and 60
    tape.Truncate(60);
    CHECK(tape.hashes.size() == 2); // The state after 60 steps stays known
    tape.Truncate(500);
    CHECK(tape.steps == 60);
}
TEST_CASE("Engine.Simulation.Tape.ReportsVersions", "[engine][simulation]") {
    const auto settings = ToyDefinition().settings;
    auto tape = RecordedTape();
    CHECK(tape.SameVersions(settings));
    tape.engineVersion = "0.0.1";
    CHECK_FALSE(tape.SameVersions(settings));
    CHECK(tape.Incompatibility(settings).empty()); // Versions are reported; the hashes decide
    tape = RecordedTape();
    tape.simulationVersion = 2;
    CHECK_FALSE(tape.SameVersions(settings));
}
