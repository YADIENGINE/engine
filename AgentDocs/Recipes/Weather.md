# Weather recipe

A night yard that turns from mist into a storm over eight seconds, with ash of the game's own, a lamp that shows the fog, and lightning.

APIs: Core/Weather/Weather.h, Core/Graphics/Atmosphere/Atmosphere.h, Core/Graphics/Atmosphere/DriftField.h, Core/Graphics/Atmosphere/FogField.h, Core/Animation/Tween.h, Core/Fx/FxStage.h (`lightning` cue). Concepts: [Atmosphere](../../Docs/Graphics/Atmosphere.md).

The order in `Draw` is the recipe: the floor, what lies behind, what stands, what lies in front, the light, what shines by itself.

<!-- recipe:Weather-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:Weather -->
```cpp
#include "AgentDocs/Recipes/Code/Weather.h"
#include <array>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

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

RecipeWeatherView::RecipeWeatherView(Float2 canvas)
    : _canvas(canvas)
    , _air(YardLook())
    , _ash(AshLook())
    , _fx(FloatRect{0.0f, 0.0f, canvas.x, canvas.y}, FloatRect{0.0f, 0.0f, canvas.x, canvas.y}, CurrentTheme().fx) {}

void RecipeWeatherView::StartStorm() { _weather.To(Weather::Storm(), StormSeconds, Curve::InOutSine); }

void RecipeWeatherView::Lightning(Float2 at) { _fx.Play("lightning", {.at = at, .color = LightningColor}); }

void RecipeWeatherView::Update(float seconds) {
    _weather.Update(seconds);
    _air.Update(seconds, _weather);
    _ash.Update(seconds, _weather.Value().wind);
    _fx.Update(seconds, seconds);
}

void RecipeWeatherView::Draw(Painter& paint, const CameraView& view) const {
    {
        auto world = paint.Camera(view);
        paint.Rect({-4000.0f, -4000.0f, 8000.0f, 8000.0f}, Fill(Floor));
    }
    _air.DrawBehind(paint, view); // Ripples and low fog pass under the characters
    {
        auto world = paint.Camera(view);
        paint.Circle(_canvas * 0.5f, 24.0f, Fill(Hero));
    }
    _ash.Draw(paint, 1.0f, view);
    _air.DrawFront(paint, view);
    // All of the above is lit like the world: fog and dust show where the lamp falls, and lightning shows them all.
    const std::array lamps{LightSource{.at = _canvas * 0.5f, .radius = 300.0f, .color = LampColor}};
    _lights.Draw(paint, {.ambient = Night, .lights = lamps, .flashes = _fx.LightFlashes()}, view);
    auto world = paint.Camera(view);
    _fx.DrawWorldLayer(paint); // The bolt shines by itself
}
```
<!-- /recipe -->

Pitfalls: Call `DrawBehind` and `DrawFront` outside `paint.Camera`, with the view the world was drawn through, shake included. Draw the air before the `LightLayer` so it is lit like the world; after it, fog and rain would glow in the dark. Give `.flashes = _fx.LightFlashes()` to the frame, or the `lightning` cue shows a bolt and lights nothing. Draw the stage's world layer after the `LightLayer`, or the bolt goes dark with the world. Use `SideViewAtmosphere(groundY)` for a world seen from the side, or fog covers the sky and ripples spread on the air. A rule that depends on the weather reads the `Weather` the game keeps, in its simulation, never the `Atmosphere`: a simulation cannot include it, and its clock is the view's. For a short burst at a point use `ParticleSystem`; a `DriftField` is for what fills the view.
