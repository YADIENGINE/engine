#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "Runtime/TimeScale.h"
#include "Input/InputBindings.h"
#include "Scene/Scene.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRunner.h"
#include "Simulation/StateHash.h"
#include "Simulation/StateVisitor.h"

namespace {
enum class Button { Jump, Fire };

class CountingSimulation final : public Simulation {
public:
    void Step(const SimulationStep& step) override {
        for (const auto& player : step.players) {
            if (player.Pressed(Button::Fire)) ++fired;
            if (player.Held(Button::Jump)) ++jumping;
        }
        ++steps;
    }
    void State(StateVisitor& state) const override {
        state.Field("steps", steps);
        state.Field("fired", fired);
        state.Field("jumping", jumping);
    }
    int steps = 0, fired = 0, jumping = 0;
};
SimulationSettings Settings() {
    SimulationSettings settings;
    settings.id = "test.counting";
    settings.maxPlayers = 2;
    settings.buttons = {"Jump", "Fire"};
    return settings;
}
// The way a game hosts a simulation: input feeds the runner, the game-domain update advances it.
class PlayScene final : public Scene {
public:
    PlayScene()
        : Scene(TimeDomain::Game)
        , runner(simulation, Settings(), SimulationStart{.players = 2})
        , bindings(2) {
        bindings.Bind(0, Button::Fire, InputKey::Space);
        bindings.Bind(0, Button::Jump, InputKey::Up);
        bindings.Bind(1, Button::Fire, InputKey::Enter);
    }
    CountingSimulation simulation;
    SimulationRunner runner;
    InputBindings bindings;

private:
    void HandleInput(const DeviceInput& input) override { runner.Feed(bindings.Read(input)); }
    void InputCancelled() override { runner.CancelInput(); }
    void Update(float seconds) override { (void)runner.Advance(seconds); }
};
DeviceInput KeyPress(InputKey key) {
    DeviceInput input;
    input.keysPressed[static_cast<std::size_t>(key)] = true;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
// One frame in Game::Run's order: input, then time through TimeScale, then the scene.
void Frame(PlayScene& scene, TimeScale& time, float realSeconds, const DeviceInput& input = {}) {
    scene.ReadInput(input);
    scene.TickReal(realSeconds);
    scene.Tick(time.Advance(realSeconds));
}
constexpr float FrameSeconds = 1.0f / 60.0f;
} // namespace

TEST_CASE("Engine.Simulation.Scene.HitstopStopsStepsButKeepsThePress", "[engine][simulation]") {
    PlayScene scene;
    scene.Activate();
    TimeScale time;
    Frame(scene, time, FrameSeconds);
    CHECK(scene.simulation.steps == 1);
    time.Hitstop(0.1f);
    Frame(scene, time, FrameSeconds, KeyPress(InputKey::Space));
    CHECK(scene.simulation.steps == 1);
    for (int i = 0; i < 5; ++i)
        Frame(scene, time, FrameSeconds);
    CHECK(scene.simulation.steps == 1);
    CHECK(scene.simulation.fired == 0);
    for (int i = 0; i < 3; ++i)
        Frame(scene, time, FrameSeconds);
    CHECK(scene.simulation.steps > 1);
    CHECK(scene.simulation.fired == 1);
    scene.Shutdown();
}
TEST_CASE("Engine.Simulation.Scene.SlowMotionLowersStepsPerSecond", "[engine][simulation]") {
    PlayScene scene;
    scene.Activate();
    TimeScale time;
    time.SlowMo(1.0f, 0.5f, 0.5f);
    for (int i = 0; i < 60; ++i)
        Frame(scene, time, FrameSeconds);
    CHECK(scene.simulation.steps >= 29);
    CHECK(scene.simulation.steps <= 31);
    for (int i = 0; i < 60; ++i)
        Frame(scene, time, FrameSeconds);
    CHECK(scene.simulation.steps >= 89);
    scene.Shutdown();
}
TEST_CASE("Engine.Simulation.Scene.TimeEffectsLeaveTheTapeAlone", "[engine][simulation]") {
    const auto play = [](bool effects) {
        PlayScene scene;
        scene.Activate();
        TimeScale time;
        std::uint64_t nextPress = 10;
        for (int frame = 0; scene.runner.StepIndex() < 300; ++frame) {
            if (effects && frame == 40) time.Hitstop(0.2f);
            if (effects && frame == 100) time.SlowMo(1.0f, 0.2f);
            // A press every 50 steps, not frames, so both runs press at the same steps.
            DeviceInput input;
            if (scene.runner.StepIndex() >= nextPress) {
                input.keysPressed[static_cast<std::size_t>(InputKey::Space)] = true;
                nextPress += 50;
            }
            Frame(scene, time, FrameSeconds, input);
        }
        auto result = std::pair{scene.runner.Tape().changes, HashState(scene.simulation)};
        scene.Shutdown();
        return result;
    };
    const auto plain = play(false), scaled = play(true);
    CHECK(plain.second == scaled.second);
    CHECK(plain.first == scaled.first);
}
TEST_CASE("Engine.Simulation.Scene.CancelReleasesHeldActions", "[engine][simulation]") {
    PlayScene scene;
    scene.Activate();
    TimeScale time;
    Frame(scene, time, FrameSeconds, KeyPress(InputKey::Up));
    CHECK(scene.simulation.jumping == 1);
    DeviceInput cancelled;
    cancelled.cancelled = true;
    scene.ReadInput(cancelled);
    scene.Tick(time.Advance(FrameSeconds));
    CHECK(scene.simulation.steps == 2);
    CHECK(scene.simulation.jumping == 1);
    scene.Shutdown();
}
TEST_CASE("Engine.InputBindings.MapsKeysToPlayers", "[engine][input]") {
    InputBindings bindings(2);
    bindings.Bind(0, Button::Fire, InputKey::Space);
    bindings.Bind(0, Button::Fire, InputKey::F);
    bindings.Bind(1, Button::Fire, InputKey::Enter);
    auto first = bindings.Read(KeyPress(InputKey::F));
    CHECK(first[0].Pressed(Button::Fire));
    CHECK(first[0].Held(Button::Fire));
    CHECK_FALSE(first[1].Pressed(Button::Fire));
    DeviceInput held;
    held.keysDown[static_cast<std::size_t>(InputKey::Enter)] = true;
    auto second = bindings.Read(held);
    CHECK_FALSE(second[0].Held(Button::Fire));
    CHECK(second[1].Held(Button::Fire));
    CHECK_FALSE(second[1].Pressed(Button::Fire));
    CHECK_THROWS(bindings.Bind(2, Button::Fire, InputKey::Space));
    CHECK_THROWS(InputBindings(0));
}
TEST_CASE("Engine.InputBindings.MouseRightActionReplaysFromTheTape", "[engine][input][simulation]") {
    const auto rightClick = [] {
        DeviceInput input;
        input.keysPressed[static_cast<std::size_t>(InputKey::MouseRight)] = true;
        input.keysDown[static_cast<std::size_t>(InputKey::MouseRight)] = true;
        return input;
    };
    CountingSimulation live;
    SimulationRunner runner(live, Settings(), SimulationStart{.players = 2});
    InputBindings bindings(2);
    bindings.Bind(1, Button::Fire, InputKey::MouseRight);
    for (int step = 0; step < 120; ++step) {
        runner.Feed(bindings.Read(step % 30 == 5 ? rightClick() : DeviceInput{}));
        REQUIRE(runner.RunStep());
    }
    CHECK(live.fired == 4);
    // The replay reads no device: the tape holds the action, not the button.
    CountingSimulation replayed;
    SimulationRunner replay(replayed, Settings(), SimulationStart{.players = 2});
    replay.Play(runner.Tape(), AfterTape::Stop);
    while (replay.RunStep()) {
    }
    CHECK(replayed.fired == 4);
    CHECK(HashState(replayed) == HashState(live));
}
