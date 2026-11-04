#include <catch2/catch_test_macros.hpp>
#include <array>
#include <memory>
#include <optional>
#include "Editor/Api/HostedSimulation.h"
#include "Editor/Internals/Session/WorkerSimulation.h"
#include "Simulation/SimulationRunner.h"
#include "Simulation/StateHash.h"
#include "Tests/Simulation/ToySimulation.h"
#include "Tests/Simulation/ToyTimeline.h"
#include "Tests/Support/TempDirectory.h"

namespace {
// The scene the worker rebuilds on a rewind: its simulation goes away with it, the host keeps the tape.
struct HostedScene {
    HostedScene(WorkerSimulation& worker, std::uint64_t seed)
        : hosted(&worker, ToyDefinition().settings, SimulationStart{.seed = seed, .players = 2}) {}
    HostedSimulation<ToySimulation> hosted;
};
std::array<PlayerInput, 2> FirePress(int player) {
    std::array<PlayerInput, 2> input;
    input[static_cast<std::size_t>(player)].SetPressed(ToyButton::Fire);
    return input;
}
void PlayRun(HostedScene& scene) {
    auto& runner = scene.hosted.Runner();
    runner.RunTo(10);
    const auto press = FirePress(0);
    runner.Feed(press);
    runner.RunTo(11);
    runner.Command("bonus", 3);
    runner.RunTo(30);
}
} // namespace

TEST_CASE("Editor.Simulation.ReportsItsState", "[engine-editor][simulation]") {
    WorkerSimulation worker;
    CHECK(worker.State().is_null());
    CHECK(worker.StepSeconds(0.5) == 0.5);
    auto scene = std::make_unique<HostedScene>(worker, 3);
    REQUIRE(worker.Runner() == &scene->hosted.Runner());
    CHECK(worker.StepSeconds(0.5) == 1.0 / 60.0);
    PlayRun(*scene);
    const auto state = worker.State();
    CHECK(state.at("step").get<std::uint64_t>() == 30);
    CHECK(state.at("tapeSteps").get<std::uint64_t>() == 30);
    CHECK(state.at("stepsPerSecond").get<int>() == 60);
    CHECK(state.at("replay").get<std::string>() == "live");
    scene.reset();
    CHECK(worker.Runner() == nullptr);
    CHECK(worker.State().is_null());
}
TEST_CASE("Editor.Simulation.RewindReplaysTheTapeWithInputAndCommands", "[engine-editor][simulation]") {
    WorkerSimulation worker;
    auto scene = std::make_unique<HostedScene>(worker, 3);
    PlayRun(*scene);
    const auto final = HashState(scene->hosted.Sim());
    const int score = scene->hosted.Sim().Score();
    const int shots = scene->hosted.Sim().Shots(0);

    worker.PrepareRewind();
    scene = std::make_unique<HostedScene>(worker, 3);
    auto& runner = scene->hosted.Runner();
    CHECK(runner.Replaying());
    CHECK(runner.Tape().steps == 30);
    runner.RunTo(5);
    CHECK(worker.State().at("replay").get<std::string>() == "playing");
    runner.RunTo(30); // What follows the rewind target stays on the tape
    CHECK(HashState(scene->hosted.Sim()) == final);
    CHECK(scene->hosted.Sim().Score() == score);
    CHECK(scene->hosted.Sim().Shots(0) == shots);
    CHECK(runner.Tape().commands.size() == 1);
    CHECK_FALSE(runner.Mismatch());
    CHECK_FALSE(runner.Replaying());
    runner.RunTo(31); // Past the tape it is live again
    CHECK(runner.Tape().steps == 31);
}
TEST_CASE("Editor.Simulation.APressAfterARewindReplacesWhatFollowed", "[engine-editor][simulation]") {
    WorkerSimulation worker;
    auto scene = std::make_unique<HostedScene>(worker, 3);
    PlayRun(*scene);
    worker.PrepareRewind();
    scene = std::make_unique<HostedScene>(worker, 3);
    auto& runner = scene->hosted.Runner();
    runner.RunTo(5);
    const auto press = FirePress(1);
    runner.Feed(press);
    CHECK(runner.Tape().steps == 5);
    CHECK(runner.Tape().commands.empty());
    CHECK_FALSE(runner.Replaying());
}
TEST_CASE("Editor.Simulation.RewindUsesCurrentParameters", "[engine-editor][simulation]") {
    WorkerSimulation worker;
    auto scene = std::make_unique<HostedScene>(worker, 3);
    PlayRun(*scene);
    const auto oldStart = scene->hosted.Runner().Tape().hashes.front();
    worker.PrepareRewind();
    scene = std::make_unique<HostedScene>(worker, 4); // The scene's seed parameter changed
    auto& runner = scene->hosted.Runner();
    CHECK(runner.Tape().start.seed == 4);
    runner.RunTo(30);
    CHECK_FALSE(runner.Mismatch()); // The old hashes were dropped and computed again
    CHECK(runner.Tape().hashes.front() != oldStart);
    CHECK(runner.Tape().steps == 30);
}
TEST_CASE("Editor.Simulation.ForgetStartsOver", "[engine-editor][simulation]") {
    WorkerSimulation worker;
    auto scene = std::make_unique<HostedScene>(worker, 3);
    PlayRun(*scene);
    worker.PrepareRewind();
    worker.Forget();
    scene = std::make_unique<HostedScene>(worker, 3);
    CHECK(scene->hosted.Runner().Tape().steps == 0);
    CHECK_FALSE(scene->hosted.Runner().Replaying());
}
TEST_CASE("Editor.Simulation.SaveAndLoadPlayAReplayWithItsOwnStart", "[engine-editor][simulation]") {
    TempDirectory directory;
    const auto file = directory.Path() / "run.replay";
    WorkerSimulation worker;
    auto scene = std::make_unique<HostedScene>(worker, 3);
    PlayRun(*scene);
    const auto final = HashState(scene->hosted.Sim());
    REQUIRE(worker.Save(file));

    REQUIRE(worker.Load(file));
    scene = std::make_unique<HostedScene>(worker, 99); // Parameters say 99; the replay's start wins
    auto& runner = scene->hosted.Runner();
    CHECK(runner.Tape().start.seed == 3);
    runner.RunTo(30);
    CHECK_FALSE(runner.Mismatch());
    CHECK(HashState(scene->hosted.Sim()) == final);
    CHECK(worker.State().at("tapeSteps").get<std::uint64_t>() == 30);

    // Rewinding a loaded replay keeps its start and its hashes, so a difference is still reported.
    worker.PrepareRewind();
    scene = std::make_unique<HostedScene>(worker, 99);
    CHECK(scene->hosted.Runner().Tape().start.seed == 3);
    CHECK(scene->hosted.Runner().Tape().hashes.size() == 31);
}
TEST_CASE("Editor.Simulation.LoadRefusesWhatCannotPlay", "[engine-editor][simulation]") {
    TempDirectory directory;
    WorkerSimulation worker;
    CHECK_FALSE(worker.Load(directory.Path() / "none.replay"));
    CHECK(worker.State().at("error").get<std::string>().find("no simulation") != std::string::npos);
    auto scene = std::make_unique<HostedScene>(worker, 3);
    CHECK_FALSE(worker.Load(directory.Path() / "none.replay"));
    CHECK(worker.State().contains("error"));

    auto other = ToyDefinition().settings;
    other.id = "engine.other";
    const auto file = directory.Path() / "other.replay";
    InputTape(other, SimulationStart{}).Save(file);
    CHECK_FALSE(worker.Load(file));
    CHECK(worker.State().at("error").get<std::string>().find("engine.other") != std::string::npos);
    CHECK(scene->hosted.Runner().Tape().simulation == "engine.toy"); // The open run is untouched
}
TEST_CASE("Editor.Simulation.WithoutAHostAHostedSimulationStillRuns", "[engine-editor][simulation]") {
    HostedSimulation<ToySimulation> hosted(nullptr, ToyDefinition().settings, SimulationStart{.seed = 1});
    hosted.Runner().RunTo(3);
    CHECK(hosted.Sim().Steps() == 3);
}

TEST_CASE("Editor.WorkerSimulation.StateNamesTheMoments", "[editor][simulation]") {
    WorkerSimulation worker;
    {
        HostedSimulation<ToySimulation> plain(&worker, ToyDefinition().settings, SimulationStart{.seed = 1, .players = 2});
        CHECK_FALSE(worker.State().contains("moments")); // A simulation that names nothing adds nothing
    }
    HostedSimulation<ToyTimelineSimulation> hosted(&worker, ToyTimelineDefinition().settings,
                                                   SimulationStart{.seed = 1, .players = 1});
    auto state = worker.State();
    REQUIRE(state.contains("moments"));
    CHECK(state.at("moments").at("boss").get<std::uint64_t>() == 180);
    CHECK_FALSE(state.at("moments").contains("attack.fire"));
    hosted.Runner().RunTo(181);
    state = worker.State();
    CHECK(state.at("moments").at("attack.fire").get<std::uint64_t>() == 210);
}
