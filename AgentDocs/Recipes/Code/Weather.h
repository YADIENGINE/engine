// [recipe:Weather-header]
#pragma once
#include "Graphics/Atmosphere/DriftField.h"
#include "Stage/Stage.h"
#include "Utilities/Geometry/Float2.h"

// A night yard that goes from mist into a storm, with ash of the game's own, a lamp that shows the fog, and lightning.
// All of it is view code on seconds and decides nothing: the stage draws the air around the world and lights it.
class RecipeWeatherView final : public StageView {
public:
    explicit RecipeWeatherView(const SceneServices& services);
    void StartStorm();
    void Lightning(Float2 at);
    // Each frame, from the scene's UpdateReal and Update.
    void UpdateReal(float realSeconds) { _stage.UpdateReal(realSeconds); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _stage.Draw(paint, *this); }

private:
    void DrawFloor(Painter& paint, const StagePass& pass) const override;
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawAir(Painter& paint, const StagePass& pass) const override;
    void DescribeLight(const Painter& paint, StageLight& light) const override;

    Float2 _hero;
    Stage _stage;    // Its Weather() is the amount; in a game whose rules care, the simulation's, snapped each frame
    DriftField _ash; // A kind of the game's own: the same field, other data
};
// [/recipe]
