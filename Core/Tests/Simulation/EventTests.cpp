#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>
#include "Simulation/Scenario.h"
#include "Simulation/ScenarioScript.h"
#include "Simulation/SimulationEventReader.h"
#include "Simulation/SimulationRunner.h"
#include "Simulation/StateDump.h"
#include "Tests/Simulation/ToySimulation.h"
#include "Utilities/Overloaded.h"

namespace {
constexpr float Frame60 = 1.0f / 60.0f;
SimulationStart OnePlayer() { return {.seed = 5, .players = 1}; }
SimulationStep StepAt(std::uint64_t index) { return {.index = index, .seconds = Frame60}; }
PlayerInput Press(ToyButton button) {
    PlayerInput input;
    input.SetPressed(button);
    input.SetHeld(button, true);
    return input;
}
void FeedOne(SimulationRunner& runner, const PlayerInput& input) { runner.Feed(std::array{input}); }
SimulationRegistry ToyRegistry() {
    SimulationRegistry registry;
    RegisterSimulations(registry);
    return registry;
}
// What a view saw: step and name of each event.
struct Seen {
    std::uint64_t step = 0;
    std::string name;
    friend bool operator==(const Seen&, const Seen&) = default;
};
template <class Span> void Collect(std::vector<Seen>& seen, Span events) {
    for (const auto& happened : events) {
        const std::string name = std::visit(Overloaded{[](const ToyShot&) { return std::string("Shot"); },
                                                       [](const ToyHit&) { return std::string("Hit"); },
                                                       [](const ToyBonus&) { return std::string("Bonus"); }},
                                            happened.event);
        seen.push_back({happened.step, name});
    }
}
} // namespace

TEST_CASE("Engine.Simulation.Events.StampStepAndOrder", "[engine][simulation]") {
    SimulationEvents<ToyEvent> events;
    events.Emit(StepAt(0), ToyShot{1});
    events.Emit(StepAt(0), ToyHit{1, 12.5f});
    events.Emit(StepAt(3), ToyBonus{});
    const auto kept = events.Kept();
    REQUIRE(kept.size() == 3);
    CHECK(kept[0].step == 0);
    CHECK(kept[1].sequence == 1);
    CHECK(kept[2].step == 3);
    CHECK(std::holds_alternative<ToyHit>(kept[1].event));
    CHECK(events.End() == 3);
    CHECK(events.FirstKept() == 0);
    CHECK(events.Since(2).size() == 1);
    CHECK(events.Since(9).empty());

    const SimulationEventLog& log = events;
    CHECK(log.NameOf(0) == "Shot");
    CHECK(log.NameOf(1) == "Hit");
    CHECK(log.NameOf(2) == "Bonus");
    CHECK(log.StepOf(2) == 3);
    StateDump fields;
    log.FieldsOf(1, fields);
    CHECK(fields.Result().At("player").AsInt64() == 1);
    CHECK(fields.Result().At("at").AsDouble() == 12.5);
    StateDump none;
    log.FieldsOf(2, none);
    CHECK(none.Result().Members().empty());
}
TEST_CASE("Engine.Simulation.Events.KeepOneFrameOfSteps", "[engine][simulation]") {
    // At 60 steps a second a frame runs at most 15 steps; 16 steps are kept.
    SimulationEvents<ToyEvent> events;
    events.Emit(StepAt(0), ToyShot{0});
    events.Emit(StepAt(15), ToyShot{0});
    CHECK(events.FirstKept() == 0); // Steps 0 to 15 are kept
    events.Emit(StepAt(16), ToyShot{0});
    CHECK(events.FirstKept() == 1);
    REQUIRE(events.Kept().size() == 2);
    CHECK(events.Kept()[0].step == 15);
    // Many events over a long run stay bounded and in order.
    for (std::uint64_t step = 18; step < 5000; ++step)
        for (int i = 0; i < 3; ++i)
            events.Emit(StepAt(step), ToyShot{i});
    CHECK(events.Kept().size() == 16U * 3U);
    CHECK(events.Kept().front().step == 4999 - 15);
    CHECK(events.Kept().back().sequence == events.End() - 1);
    CHECK(events.End() == 3 + 3 * (5000 - 18));
}
TEST_CASE("Engine.Simulation.Events.ReadersAreIndependent", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    SimulationEventReader<ToyEvent> sounds, popups;
    FeedOne(runner, Press(ToyButton::Fire));
    (void)runner.Advance(Frame60);
    const auto heard = sounds.Read(simulation.Events(), runner).size(); // The shot, and a hit if it landed
    CHECK(heard >= 1);
    CHECK(sounds.Read(simulation.Events(), runner).empty()); // Read once
    CHECK(popups.Read(simulation.Events(), runner).size() == heard);
    sounds.Skip(simulation.Events());
    CHECK(sounds.Missed() == 0);
}
TEST_CASE("Engine.Simulation.Events.AFrameOfSeveralStepsLosesNone", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    ToySimulation simulation(OnePlayer());
    SimulationRunner runner(simulation, definition.settings, OnePlayer());
    SimulationEventReader<ToyEvent> reader;
    std::vector<Seen> seen;
    // A press per step, read only once per long frame: a counter would see one change.
    for (int frame = 0; frame < 4; ++frame) {
        for (int step = 0; step < 15; ++step) {
            FeedOne(runner, Press(ToyButton::Fire));
            (void)runner.RunStep();
        }
        Collect(seen, reader.Read(simulation.Events(), runner));
    }
    std::size_t shots = 0;
    for (const auto& item : seen)
        shots += item.name == "Shot" ? 1U : 0U;
    CHECK(shots == 60);
    CHECK(seen.front().step == 0);
    CHECK(seen.back().step == 59);
    CHECK(reader.Missed() == 0);
}
TEST_CASE("Engine.Simulation.Events.AReaderLeftBehindCountsWhatItMissed", "[engine][simulation]") {
    SimulationEvents<ToyEvent> events;
    SimulationEventReader<ToyEvent> reader;
    for (std::uint64_t step = 0; step < 100; ++step)
        events.Emit(StepAt(step), ToyShot{0});
    CHECK(reader.ReadAll(events).size() == 16);
    CHECK(reader.Missed() == 84);
}
TEST_CASE("Engine.Simulation.Events.CatchingUpIsSilent", "[engine][simulation]") {
    auto definition = ToyDefinition();
    ToySimulation recorded(OnePlayer());
    SimulationRunner recorder(recorded, definition.settings, OnePlayer());
    for (int step = 0; step < 40; ++step) {
        FeedOne(recorder, Press(ToyButton::Fire));
        (void)recorder.RunStep();
    }
    // A replay fast-forwarded to step 30: those steps play nothing, the steps after it play.
    ToySimulation replayed(OnePlayer());
    SimulationRunner player(replayed, definition.settings, OnePlayer());
    player.Play(recorder.Tape(), AfterTape::Stop);
    SimulationEventReader<ToyEvent> sounds, score;
    player.RunTo(30);
    CHECK(sounds.Read(replayed.Events(), player).empty());
    const auto caught = score.ReadAll(replayed.Events()); // Only what is kept; a total belongs in the state
    REQUIRE_FALSE(caught.empty());
    CHECK(caught.front().step == 14);
    CHECK(caught.back().step == 29);
    CHECK(player.Advance(Frame60) == 1);
    const auto played = sounds.Read(replayed.Events(), player);
    REQUIRE_FALSE(played.empty());
    CHECK(played.front().step == 30);
    CHECK(played.back().step == 30);
    // The editor's seek: silent until it arrives, then the arrival step.
    player.SilenceEventsUntil(UINT64_MAX);
    (void)player.Advance(Frame60);
    (void)player.Advance(Frame60);
    player.SilenceEventsUntil(player.StepIndex());
    CHECK(sounds.Read(replayed.Events(), player).empty());
    (void)player.Advance(Frame60);
    CHECK_FALSE(sounds.Read(replayed.Events(), player).empty());
}
TEST_CASE("Engine.Simulation.Events.ReplayEmitsWhatLivePlayEmitted", "[engine][simulation]") {
    const auto definition = ToyDefinition();
    const SimulationStart start{.seed = 11, .players = 2};
    ToySimulation live(start);
    SimulationRunner recorder(live, definition.settings, start);
    SimulationEventReader<ToyEvent> liveReader;
    std::vector<Seen> liveSeen;
    for (int frame = 0; frame < 600; ++frame) {
        PlayerInput first, second;
        if (frame % 7 == 0) first = Press(ToyButton::Fire);
        if (frame % 11 == 0) second = Press(ToyButton::Fire);
        first.SetHeld((frame / 50) % 2 == 0 ? ToyButton::Right : ToyButton::Left, true);
        recorder.Feed(std::array{first, second});
        if (frame % 90 == 0) recorder.Command("bonus", 3);
        (void)recorder.Advance(frame % 3 == 0 ? Frame60 * 2.0f : Frame60 * 0.5f); // Uneven frames
        Collect(liveSeen, liveReader.Read(live.Events(), recorder));
    }
    ToySimulation replayed(start);
    SimulationRunner player(replayed, definition.settings, start);
    player.Play(recorder.Tape(), AfterTape::Stop);
    SimulationEventReader<ToyEvent> replayReader;
    std::vector<Seen> replaySeen;
    while (!player.Finished()) {
        (void)player.Advance(Frame60);
        Collect(replaySeen, replayReader.Read(replayed.Events(), player));
    }
    CHECK(liveSeen.size() > 50);
    CHECK(replaySeen == liveSeen);
}
TEST_CASE("Engine.Simulation.Events.ScenarioSeesEvents", "[engine][simulation]") {
    Scenario scenario(ToyDefinition(), {.seed = 1, .players = 2});
    scenario.Tap("Fire", 1);
    scenario.Run(3);
    scenario.Command("bonus", 1);
    scenario.Run(1);
    const auto& events = scenario.Events();
    REQUIRE(events.size() >= 2);
    CHECK(events.front().name == "Shot");
    CHECK(events.front().step == 0);
    CHECK(events.front().fields.At("player").AsInt64() == 1);
    CHECK(events.back().name == "Bonus");
    CHECK(events.back().step == 3);
}
TEST_CASE("Engine.Simulation.Events.ScriptExpectsEvents", "[engine][simulation]") {
    const auto passing = RunScenarioScript(ToyRegistry(), JsonIO::Parse(R"({
        "simulation": "engine.toy", "players": 2,
        "script": [
            {"run": 10}, {"expectEvent": "Shot", "count": 0},
            {"tap": "Fire", "player": 1}, {"run": 1},
            {"expectEvent": "Shot", "fields": {"player": 1}},
            {"expectEvent": "Shot", "count": 0},
            {"command": "bonus", "value": 2}, {"untilEvent": "Bonus", "limit": 5},
            {"expect": {"steps": 12}}
        ]})"),
                                           ScenarioScriptOptions{.listEvents = true});
    CHECK(passing.passed);
    CHECK(passing.report.At("events").At("Shot").AsUInt64() == 1);
    CHECK(passing.report.At("events").At("Bonus").AsUInt64() == 1);
    const auto& list = passing.report.At("eventList").Elements(); // A Hit may follow the Shot
    REQUIRE(list.size() >= 2);
    CHECK(list.back().At("name").AsString() == "Bonus");
    CHECK(list[0].At("name").AsString() == "Shot");
    CHECK(list[0].At("step").AsUInt64() == 10);
    CHECK(list[0].At("fields").At("player").AsInt64() == 1);

    const auto failing = RunScenarioScript(ToyRegistry(), JsonIO::Parse(R"({
        "simulation": "engine.toy", "players": 2,
        "script": [
            {"tap": "Fire"}, {"run": 1}, {"expectEvent": "Shot", "fields": {"player": 1}},
            {"untilEvent": "Bonus", "limit": 4}
        ]})"));
    CHECK_FALSE(failing.passed);
    CHECK_FALSE(failing.report.Members().contains("eventList"));
    const auto& failures = failing.report.At("failures").Elements();
    REQUIRE(failures.size() == 2);
    CHECK(failures[0].At("event").AsString() == "Shot");
    CHECK(failures[0].At("actual").AsUInt64() == 0);
    CHECK(failures[1].At("untilEvent").AsString() == "Bonus");
    CHECK(failures[1].At("step").AsUInt64() == 5);
}
