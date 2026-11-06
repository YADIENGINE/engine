// [recipe:SimulationPlay-header]
#pragma once
#include <cstdint>
#include <string_view>
#include <variant>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Runtime/TimeScale.h"
#include "Input/InputBindings.h"
#include "Scene/Scene.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationEventReader.h"
#include "Simulation/SimulationEvents.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/SimulationRunner.h"
#include "Simulation/StateVisitor.h"
#include "Simulation/StepSmoothed.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"

enum class RecipeButton { Left, Right, Fire };

// What the rules tell the view happened: plain values. The view plays sounds and effects for them.
struct RecipeHit {
    static constexpr std::string_view Name = "Hit"; // How tools name it: sim run --events, expectEvent
    float x = 0.0f;                                  // Tools show fields by their names
};
struct RecipeMiss {
    static constexpr std::string_view Name = "Miss";
};
using RecipeEvent = std::variant<RecipeHit, RecipeMiss>;

// Everything that decides what happens next, in one plain struct: State visits all of it, so a field added here is
// hashed and dumped without another line.
struct RecipePlayRun {
    Rng rng;
    StepSmoothed<float> shipX; // State keeps its current value; the view draws between steps
    float rockX = 0.0f;
    int score = 0;
};

// The rules: plain state, in Game/Simulation. No drawing, audio or clock.
class RecipePlaySimulation final : public Simulation {
public:
    explicit RecipePlaySimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] float ShipX(float alpha) const { return _run.shipX.At(alpha); }
    [[nodiscard]] float RockX() const { return _run.rockX; }
    [[nodiscard]] int Score() const { return _run.score; }
    // The view reads these to start hitstop; the simulation does not touch TimeScale.
    [[nodiscard]] const SimulationEvents<RecipeEvent>& Events() const { return _events; }
    [[nodiscard]] const SimulationEventLog* EventLog() const override { return &_events; }

private:
    RecipePlayRun _run;
    SimulationEvents<RecipeEvent> _events; // Output, not state: not in State
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
    SimulationEventReader<RecipeEvent> _events;
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
// [/recipe]
