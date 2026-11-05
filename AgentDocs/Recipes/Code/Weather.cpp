// [recipe:Weather]
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
// [/recipe]
