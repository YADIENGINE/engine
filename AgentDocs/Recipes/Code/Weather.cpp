// [recipe:Weather]
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
// [/recipe]
