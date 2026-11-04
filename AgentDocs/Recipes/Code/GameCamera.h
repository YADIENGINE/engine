// [recipe:GameCamera-header]
#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include "Camera/Camera2D.h"
#include "Camera/Cameras.h"
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Fx/FxStage.h"
#include "Input/InputBindings.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/StepSmoothed.h"

// 1. A camera in the view: two players share one screen, framed together, or split it. The camera decides nothing,
// so it lives on seconds, beside the effects.
class RecipeCameraView {
public:
    explicit RecipeCameraView(const FloatRect& canvas);
    void SetSplit(bool split);
    void SetGuides(bool shown) { _guides = shown; }
    // Each frame: how each player moves (-1..1 per axis), the mouse on the canvas, and a click.
    void Update(float seconds, const std::array<Float2, 2>& moves, std::optional<Float2> mouse, bool fire);
    void Draw(Painter& paint) const;
    [[nodiscard]] const Cameras& Views() const { return _cameras; }
    [[nodiscard]] Float2 Player(std::size_t index) const { return _players[index]; }
    [[nodiscard]] Float2 Aim() const { return _aim; }

private:
    void DrawWorld(Painter& paint) const;
    std::array<Float2, 2> _players;
    Float2 _aim{};
    Cameras _cameras;
    FxStage _fx;
    bool _split = false;
    bool _guides = false;
};

// 2. A camera in the rules: the view never scrolls back, and enemies wake when it first shows them. It decides
// gameplay, so it steps with the simulation, goes into State, and is drawn between steps.
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
// The view of the scroller: the world through the rules' camera, at the runner's alpha.
void DrawRecipeScroller(Painter& paint, const RecipeScrollerSimulation& simulation, float alpha, bool guides);

// Both in the editor. Parameters switch split screen and the camera's guides.
class RecipeCameraEditorScene final : public EditorScene {
public:
    RecipeCameraEditorScene(const SceneServices& services, bool split, bool guides);

private:
    void HandleInput(const DeviceInput& input) override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    RecipeCameraView _view;
    std::array<Float2, 2> _moves{};
    std::optional<Float2> _mouse;
    bool _fire = false;
};
class RecipeScrollerEditorScene final : public EditorScene {
public:
    RecipeScrollerEditorScene(const SceneServices& services, bool guides);

private:
    void HandleInput(const DeviceInput& input) override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    HostedSimulation<RecipeScrollerSimulation> _hosted;
    InputBindings _bindings;
    bool _guides;
};
[[nodiscard]] SceneDefinition RecipeCameraSceneDefinition();
[[nodiscard]] SceneDefinition RecipeScrollerSceneDefinition();
// [/recipe]
