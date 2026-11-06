# Weather recipe

A night yard that turns from mist into a storm over eight seconds, with ash of the game's own, a lamp that shows the fog, and lightning.

APIs: Core/Stage/Stage.h (`StageSettings::atmosphere`, `Weather`), Core/Stage/StageView.h (`DrawAir`), Core/Weather/Weather.h, Core/Graphics/Atmosphere/Atmosphere.h, Core/Graphics/Atmosphere/DriftField.h, Core/Graphics/Atmosphere/FogField.h, Core/Animation/Tween.h, Core/Fx/FxStage.h (`lightning` cue). Concepts: [Atmosphere](../../Docs/Graphics/Atmosphere.md), [Stage](../../Docs/Graphics/Stage.md).

The stage keeps the order: the floor, what lies behind, what stands, the game's own air, what lies in front, the light, what shines by itself. The view only fills its passes.

<!-- recipe:Weather-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:Weather -->
```cpp
#include "AgentDocs/Recipes/Code/Weather.h"
#include "Graphics/Paint/Painter.h"
#include "Scene/SceneServices.h"

namespace {
constexpr Color4 Floor = Rgb(0x6c7a66);
constexpr Color4 Hero = Rgb(0xd9694a);
constexpr Color4 LampColor = Rgb(0xffd9a0);
constexpr Color4 Night = Rgb(0x141a2c);
constexpr Color4 LightningColor = Rgb(0xdce6ff);
constexpr float StormSeconds = 8.0f;

AtmosphereLook YardLook() {
    AtmosphereLook look = TopDownAtmosphere();
    look.fogBehind.color = {170, 190, 215, 170}; // Colder than the default
    return look;
}

DriftLook AshLook() {
    return {.density = 0.6f,
            .fall = {0.0f, 24.0f},
            .size = {1.5f, 3.0f},
            .spin = {-2.0f, 2.0f},
            .sway = 18.0f,
            .swayRate = 0.8f,
            .shape = ParticleShape::Square,
            .color = {60, 56, 54, 220}};
}
} // namespace

RecipeWeatherView::RecipeWeatherView(const SceneServices& services)
    : _hero(ToFloat2(services.canvas) * 0.5f)
    // Light and air are switched on here. The air is drawn before the light, so it is lit like the world: fog and dust
    // show where the lamp falls, and lightning shows them all.
    , _stage(services, {.light = LightLayerOptions{}, .atmosphere = YardLook()})
    , _ash(AshLook()) {
    _stage.Weather().Snap(Weather::Mist());
    _stage.Cameras().Main().Follow(_hero);
    _stage.Cameras().Snap();
}

void RecipeWeatherView::StartStorm() { _stage.Weather().To(Weather::Storm(), StormSeconds, Curve::InOutSine); }

void RecipeWeatherView::Lightning(Float2 at) { _stage.Fx().Play("lightning", {.at = at, .color = LightningColor}); }

void RecipeWeatherView::Update(float seconds) {
    _stage.Update(seconds); // Moves the weather, the air and the bolt
    _ash.Update(seconds, _stage.Weather().Value().wind);
}

void RecipeWeatherView::DrawFloor(Painter& paint, const StagePass&) const {
    paint.Rect({-4000.0f, -4000.0f, 8000.0f, 8000.0f}, Fill(Floor)); // Ripples and low fog pass over it
}

void RecipeWeatherView::DrawWorld(Painter& paint, const StagePass&) const { paint.Circle(_hero, 24.0f, Fill(Hero)); }

void RecipeWeatherView::DrawAir(Painter& paint, const StagePass& pass) const { _ash.Draw(paint, 1.0f, pass.view); }

void RecipeWeatherView::DescribeLight(const Painter&, StageLight& light) const {
    light.ambient = Night;
    light.lights.push_back({.at = _hero, .radius = 300.0f, .color = LampColor});
}
```
<!-- /recipe -->

Pitfalls: Change the amount through `Weather()` (`To` over time, `Snap` for a simulation's weather each frame); the stage moves the air with it on game time. A kind of the game's own goes in `DrawAir`, outside `paint.Camera`, with `pass.view`, and its own `Update`. Use `SideViewAtmosphere(groundY)` for a world seen from the side, or fog covers the sky and ripples spread on the air. A rule that depends on the weather reads the `Weather` the game keeps, in its simulation, never the `Atmosphere`: a simulation cannot include it, and its clock is the view's. For a short burst at a point use `ParticleSystem`; a `DriftField` is for what fills the view.
