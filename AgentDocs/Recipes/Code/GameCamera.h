// [recipe:GameCamera-header]
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include "Camera/Camera2D.h"
#include "Editor/Api/SceneRegistry.h"
#include "Input/InputBindings.h"
#include "Scene/Scene.h"
#include "Simulation/HostedSimulation.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/StepSmoothed.h"
#include "Stage/Stage.h"

// 1. A camera in the view: two players share one screen, framed together, or split it. The camera decides nothing,
// so it lives on seconds in the stage, beside the effects; each view of a split draws through the same passes.
class RecipeCameraView final : public StageView {
public:
    explicit RecipeCameraView(const SceneServices& services);
    void SetSplit(bool split);
    void SetGuides(bool shown) { _stage.SetGuides({.cameras = shown}); }
    // Each frame, from the scene's two hooks: real time, and game time with how each player moves (-1..1 per axis),
    // the mouse on the canvas, and a click.
    void UpdateReal(float realSeconds) { _stage.UpdateReal(realSeconds); }
    void Update(float seconds, const std::array<Float2, 2>& moves, std::optional<Float2> mouse, bool fire);
    void Draw(Painter& paint) const { _stage.Draw(paint, *this); }
    [[nodiscard]] const Cameras& Views() const { return _stage.Cameras(); }
    [[nodiscard]] Float2 Player(std::size_t index) const { return _players[index]; }
    [[nodiscard]] Float2 Aim() const { return _aim; }

private:
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    std::array<Float2, 2> _players;
    Float2 _aim{};
    Stage _stage;
    bool _split = false;
};

// 2. A camera in the rules: the view never scrolls back, and enemies wake when it first shows them. It decides
// gameplay, so it steps with the simulation, goes into State, and the stage draws through its view between steps.
enum class RecipeScrollButton { Left, Right };
class RecipeScrollerSimulation final : public Simulation {
public:
    static constexpr std::size_t EnemyCount = 8;
    explicit RecipeScrollerSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] float ShipX(float alpha) const { return _shipX.At(alpha); }
    [[nodiscard]] const Camera2D& Camera() const { return _camera; }
    [[nodiscard]] bool Awake(std::size_t enemy) const { return _awake[enemy]; }
    [[nodiscard]] float ScrolledTo() const { return _scrolledTo; }

private:
    StepSmoothed<float> _shipX;
    Camera2D _camera;
    float _scrolledTo = 0.0f; // The left edge the view never goes back past
    std::array<bool, EnemyCount> _awake{};
};
[[nodiscard]] SimulationDefinition RecipeScrollerDefinition();

// The scenes the game shows and the editor opens. Parameters switch split screen and the camera's guides.
class RecipeCameraScene final : public Scene {
public:
    RecipeCameraScene(const SceneServices& services, bool split, bool guides);

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void UpdateReal(float realSeconds) override { _view.UpdateReal(realSeconds); }
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    RecipeCameraView _view;
    std::array<Float2, 2> _moves{};
    std::optional<Float2> _mouse;
    bool _fire = false;
};
// The scroller's view of the rules: the stage draws the world through the simulation's camera, at the runner's alpha.
class RecipeScrollerScene final : public Scene, StageView {
public:
    RecipeScrollerScene(const SceneServices& services, bool guides);
    [[nodiscard]] const RecipeScrollerSimulation& Rules() const { return _hosted.Sim(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void UpdateReal(float realSeconds) override { _stage.UpdateReal(realSeconds); }
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawHud(Painter& paint) const override;
    void ShowRules();
    HostedSimulation<RecipeScrollerSimulation> _hosted;
    InputBindings _bindings;
    Stage _stage;
    bool _guides;
};
[[nodiscard]] SceneDefinition RecipeCameraSceneDefinition();
[[nodiscard]] SceneDefinition RecipeScrollerSceneDefinition();
// [/recipe]
