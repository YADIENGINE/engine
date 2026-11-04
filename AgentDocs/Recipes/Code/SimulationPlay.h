// [recipe:SimulationPlay-header]
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
// [/recipe]
