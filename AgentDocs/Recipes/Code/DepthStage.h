// [recipe:DepthStage-header]
#pragma once
#include <array>
#include <cstddef>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Stage/DepthOrder.h"
#include "Stage/Stage.h"

// A 2.5D street seen from the side and a little above: two players walk among trees, jump with shadows under them and
// leave dust where they land; hills drift behind at their own depth. Split screen gives each player a view, drawn
// through the same passes. View code on seconds; in a game the walkers would be a simulation's, read here.
struct RecipeWalker {
    Float2 ground{};     // Where it stands: what it sorts by and where its shadow lies
    float height = 0.0f; // How far above the ground it is
    float rise = 0.0f;   // Upward speed, world units per second
};

// What the world pass sorts: a key of the game's own, so several kinds share one order.
struct RecipeStreetThing {
    enum class Kind { Walker, Tree };
    Kind kind = Kind::Walker;
    std::size_t index = 0;
};

class RecipeStreetScene final : public Scene, StageView {
public:
    RecipeStreetScene(const SceneServices& services, bool split);
    [[nodiscard]] const RecipeWalker& Walker(std::size_t player) const { return _walkers[player]; }
    [[nodiscard]] const Cameras& Views() const { return _stage.Cameras(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void UpdateReal(float realSeconds) override { _stage.UpdateReal(realSeconds); } // Even in a pause
    void Update(float seconds) override;
    void Draw(Painter& paint) const override { _stage.Draw(paint, *this); }
    // The passes this scene fills; the stage calls them in its order, for each view.
    void DrawBackground(Painter& paint, const StagePass& pass) const override;
    void DrawFloor(Painter& paint, const StagePass& pass) const override;
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawHud(Painter& paint) const override;
    void SetSplit(bool split);

    Stage _stage;
    std::array<RecipeWalker, 2> _walkers;
    std::array<Float2, 2> _moves{};
    std::array<bool, 2> _jumps{};
    bool _split = false;
    mutable DepthOrder<RecipeStreetThing> _order; // Refilled in each world pass; a member keeps its memory
};

[[nodiscard]] SceneDefinition RecipeStreetSceneDefinition();
// [/recipe]
