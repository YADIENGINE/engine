Write gameplay rules as a deterministic simulation, host them in a scene and in the editor, and test them without a window.

APIs: Core/Simulation/Simulation.h, Core/Simulation/SimulationRunner.h, Core/Simulation/SimulationRegistry.h, Core/Input/InputBindings.h, Editor/Api/HostedSimulation.h.

Put the rules in Game/Simulation/ (the project's CMake builds them into a library for the game, worker and `python Engine sim`) and implement `void RegisterSimulations(SimulationRegistry& registry) { registry.Add(RecipePlayDefinition()); }` there. Views and scenes stay outside that folder. This example keeps them in one file only to read in one place.

<!-- recipe:SimulationPlay-header -->
```cpp
#pragma once
#include <cstdint>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Runtime/TimeScale.h"
#include "Input/InputBindings.h"
#include "Scene/Scene.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/SimulationRunner.h"
#include "Simulation/StepSmoothed.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"

enum class RecipeButton { Left, Right, Fire };

// The rules: plain state, in Game/Simulation. No drawing, audio or clock.
class RecipePlaySimulation final : public Simulation {
public:
    explicit RecipePlaySimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] float ShipX(float alpha) const { return _shipX.At(alpha); }
    [[nodiscard]] float RockX() const { return _rockX; }
    [[nodiscard]] int Score() const { return _score; }
    // A view or scene reads this to start hitstop; the simulation does not touch TimeScale.
    [[nodiscard]] std::uint64_t HitCount() const { return _hits; }

private:
    Rng _rng;
    StepSmoothed<float> _shipX;
    float _rockX = 0.0f;
    int _score = 0;
    std::uint64_t _hits = 0;
};
[[nodiscard]] const SimulationSettings& RecipePlaySettings();
[[nodiscard]] SimulationDefinition RecipePlayDefinition();

// The game's scene: feeds input, advances by scaled seconds, draws between steps.
class RecipePlayScene final : public Scene {
public:
    RecipePlayScene(TimeScale& time, std::uint64_t seed);
    ~RecipePlayScene() override;
    [[nodiscard]] const RecipePlaySimulation& Sim() const { return _simulation; }
    [[nodiscard]] const SimulationRunner& Runner() const { return _runner; }

protected:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

private:
    TimeScale& _time;
    SimulationStart _start;
    RecipePlaySimulation _simulation;
    SimulationRunner _runner;
    InputBindings _bindings;
    std::uint64_t _hitsSeen = 0;
};

// The same rules in the editor: services.simulation gives the worker the tape, so seek, replay and Step work in steps.
class RecipePlayEditorScene final : public EditorScene {
public:
    RecipePlayEditorScene(const SceneServices& services, std::uint64_t seed);

private:
    void HandleInput(const DeviceInput& input) override;
    [[nodiscard]] std::string ExecuteAction(const std::string& id) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    HostedSimulation<RecipePlaySimulation> _hosted;
    InputBindings _bindings;
};
[[nodiscard]] SceneDefinition RecipePlaySceneDefinition();
```
<!-- /recipe -->

<!-- recipe:SimulationPlay -->
```cpp
#include "AgentDocs/Recipes/Code/SimulationPlay.h"
#include <cmath>
#include <memory>
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr float ShipSpeed = 240.0f;
constexpr float FieldWidth = 800.0f;
constexpr float HitRange = 30.0f;
constexpr float DrawY = 400.0f;
constexpr float DrawSize = 16.0f;
constexpr float HitstopSeconds = 0.05f;
constexpr Color4 ShipColor{120, 220, 255, 255};
constexpr Color4 RockColor{255, 180, 90, 255};

void DrawField(Painter& paint, float shipX, float rockX) {
    paint.Circle({shipX, DrawY}, DrawSize, Fill(ShipColor));
    paint.Circle({rockX, DrawY - DrawSize * 4.0f}, DrawSize, Fill(RockColor));
}
InputBindings MakeBindings() {
    InputBindings bindings(1);
    bindings.Bind(0, RecipeButton::Left, InputKey::A);
    bindings.Bind(0, RecipeButton::Right, InputKey::D);
    bindings.Bind(0, RecipeButton::Fire, InputKey::Space);
    return bindings;
}
} // namespace

RecipePlaySimulation::RecipePlaySimulation(const SimulationStart& start)
    : _rng(start.seed)
    , _rockX(_rng.Float(0.0f, FieldWidth)) {}
void RecipePlaySimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    float direction = 0.0f;
    if (input.Held(RecipeButton::Left)) direction -= 1.0f;
    if (input.Held(RecipeButton::Right)) direction += 1.0f;
    _shipX.Set(_shipX.Current() + direction * ShipSpeed * step.seconds);
    if (input.Pressed(RecipeButton::Fire) && std::fabs(_shipX.Current() - _rockX) < HitRange) {
        ++_score;
        ++_hits;
        _rockX = _rng.Float(0.0f, FieldWidth);
    }
    for (const auto& command : step.commands)
        if (command.id == "add_score") _score += static_cast<int>(command.value);
}
void RecipePlaySimulation::State(StateVisitor& state) const {
    state.Field("rng", _rng);
    state.Field("shipX", _shipX.Current());
    state.Field("rockX", _rockX);
    state.Field("score", _score);
    state.Field("hits", _hits);
}
const SimulationSettings& RecipePlaySettings() {
    static const SimulationSettings settings = [] {
        SimulationSettings made;
        made.id = "myproject.play";
        made.buttons = {"Left", "Right", "Fire"};
        return made;
    }();
    return settings;
}
SimulationDefinition RecipePlayDefinition() {
    return {RecipePlaySettings(),
            [](const SimulationStart& start) { return std::make_unique<RecipePlaySimulation>(start); }};
}

RecipePlayScene::RecipePlayScene(TimeScale& time, std::uint64_t seed)
    : Scene(TimeDomain::Game)
    , _time(time)
    , _start{.seed = seed, .players = 1}
    , _simulation(_start)
    , _runner(_simulation, RecipePlaySettings(), _start)
    , _bindings(MakeBindings()) {}
RecipePlayScene::~RecipePlayScene() { ShutdownNoThrow(); }
void RecipePlayScene::HandleInput(const DeviceInput& input) { _runner.Feed(_bindings.Read(input)); }
void RecipePlayScene::InputCancelled() { _runner.CancelInput(); }
void RecipePlayScene::Update(float seconds) {
    (void)_runner.Advance(seconds); // TimeScale already applied: hitstop and slow motion only change steps per second
    if (_simulation.HitCount() != _hitsSeen) {
        _hitsSeen = _simulation.HitCount();
        _time.Hitstop(HitstopSeconds);
    }
}
void RecipePlayScene::Draw(Painter& paint) const {
    DrawField(paint, _simulation.ShipX(_runner.Alpha()), _simulation.RockX());
}

RecipePlayEditorScene::RecipePlayEditorScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipePlaySettings(), SimulationStart{.seed = seed, .players = 1})
    , _bindings(MakeBindings()) {}
void RecipePlayEditorScene::HandleInput(const DeviceInput& input) {
    if (input.cancelled)
        _hosted.Runner().CancelInput();
    else
        _hosted.Runner().Feed(_bindings.Read(input));
}
std::string RecipePlayEditorScene::ExecuteAction(const std::string& id) {
    if (id == "add_score") _hosted.Runner().Command("add_score", 5); // On the tape, so a rewind plays it again
    return {};
}
void RecipePlayEditorScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipePlayEditorScene::Draw(Painter& paint) const {
    DrawField(paint, _hosted.Sim().ShipX(_hosted.Runner().Alpha()), _hosted.Sim().RockX());
}
SceneDefinition RecipePlaySceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.play";
    definition.name = "Play";
    definition.category = "MyProject";
    definition.parameters = {
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{1}, 0.0, 1000000.0, SceneApplyMode::OnReset}};
    definition.actions = {{"add_score", "Add score", "A command on the tape.", true}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipePlayEditorScene>(
            services, static_cast<std::uint64_t>(std::get<std::int64_t>(parameters.at("seed"))));
    };
    return definition;
}
```
<!-- /recipe -->

Test it without a window: `python Engine sim run scenario.json --out run.replay`, with

```json
{ "simulation": "myproject.play", "seed": 1, "script": [ { "hold": "Right" }, { "run": 60 },
  { "expect": { "shipX": { "min": 239, "max": 241 } } } ] }
```

Pitfalls: Everything that decides gameplay comes from SimulationStart or the steps. Name every deciding field in State, the Rng included. A paused scene neither feeds nor advances. The simulation asks for hitstop through state the scene reads; the scene calls TimeScale. An editor action that changes gameplay calls Runner().Command, so it is on the tape; one that only changes the view is a parameter.

Hits and overlaps inside the rules: [Collision recipe](Collision.md).

Concepts: Docs/Simulation.md.
