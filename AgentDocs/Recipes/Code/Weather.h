// [recipe:Weather-header]
#pragma once
#include "Animation/Tween.h"
#include "Camera/CameraView.h"
#include "Fx/FxStage.h"
#include "Graphics/Atmosphere/Atmosphere.h"
#include "Graphics/Atmosphere/DriftField.h"
#include "Graphics/Light/LightLayer.h"
#include "Utilities/Geometry/Float2.h"
#include "Weather/Weather.h"

// A night yard that goes from mist into a storm, with ash of the game's own, a lamp that shows the fog, and lightning.
// All of it is view code on seconds and decides nothing.
class RecipeWeatherView {
public:
    explicit RecipeWeatherView(Float2 canvas);
    // The player's choice, Game::Effects(): lightning follows it through the stage.
    void SetEffects(const FxQuality& effects) { _fx.SetQuality(effects); }
    void StartStorm();
    void Lightning(Float2 at);
    void Update(float seconds);
    void Draw(Painter& paint, const CameraView& view) const;

private:
    Float2 _canvas;
    SecondsTween<Weather> _weather{Weather::Mist()}; // In a game whose rules care: the simulation's, read by the view
    Atmosphere _air;
    DriftField _ash; // A kind of the game's own: the same field, other data
    LightLayer _lights;
    FxStage _fx;
};
// [/recipe]
