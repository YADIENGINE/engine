#include "Graphics/Atmosphere/Atmosphere.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"

namespace {
constexpr Color4 RainColor{190, 210, 235, 120};
constexpr Color4 SnowColor{240, 245, 255, 215};
constexpr Color4 DustColor{235, 225, 200, 170};
constexpr Color4 FogColor{198, 204, 216, 170};
constexpr float FarParallax = 0.86f;  // Behind what stands in the world
constexpr float NearParallax = 1.22f; // In front of it

DriftLook SnowLayer(float density, FloatRange size, float parallax, Color4 color) {
    return {.density = density,
            .fall = {0.0f, 58.0f},
            .pace = {0.75f, 1.25f},
            .size = size,
            .sway = 14.0f,
            .swayRate = 1.3f,
            .parallax = parallax,
            .color = color};
}

DriftLook DustLayer(float density, FloatRange size, float parallax, Color4 color) {
    return {.density = density,
            .fall = {4.0f, -3.0f},
            .windShare = 0.5f,
            .pace = {0.6f, 1.4f},
            .size = size,
            .sway = 9.0f,
            .swayRate = 0.5f,
            .twinkle = 0.7f,
            .parallax = parallax,
            .color = color};
}

DriftLook RainLayer(float density, Float2 fall, float seconds, FloatRange size, float parallax, Color4 color) {
    return {.density = density,
            .fall = fall,
            .pace = {0.85f, 1.15f},
            .size = size,
            .streakSeconds = seconds,
            .parallax = parallax,
            .shape = ParticleShape::Spark,
            .color = color};
}

float Amount(float value) { return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f; }
} // namespace

AtmosphereLook TopDownAtmosphere() {
    return {
        .ripples = {.density = 1.1f},
        .fogBehind = {.color = FogColor, .wavelength = 560.0f},
        .dustBehind = DustLayer(1.4f, {0.7f, 1.3f}, 1.0f, ScaleAlpha(DustColor, 0.7f)),
        .snowBehind = SnowLayer(3.6f, {1.3f, 2.3f}, FarParallax, ScaleAlpha(SnowColor, 0.65f)),
        .rainBehind = RainLayer(4.0f, {0.0f, 620.0f}, 0.035f, {1.0f, 1.3f}, FarParallax, ScaleAlpha(RainColor, 0.6f)),
        .dustFront = DustLayer(0.7f, {1.2f, 2.2f}, NearParallax, DustColor),
        .snowFront = SnowLayer(2.4f, {2.4f, 4.2f}, NearParallax, SnowColor),
        .rainFront = RainLayer(3.5f, {0.0f, 800.0f}, 0.04f, {1.2f, 1.7f}, NearParallax, RainColor),
        .fogFront = {.color = ScaleAlpha(FogColor, 0.45f), .wavelength = 860.0f, .parallax = 1.35f},
    };
}

AtmosphereLook SideViewAtmosphere(float groundY) {
    AtmosphereLook look = TopDownAtmosphere();
    look.ripples.density = 0.0f;
    look.rainBehind.fall = {0.0f, 780.0f};
    look.rainBehind.streakSeconds = 0.045f;
    look.rainFront.fall = {0.0f, 980.0f};
    look.rainFront.streakSeconds = 0.05f;
    look.fogBehind.band = FogBand{.top = groundY - 170.0f, .bottom = groundY + 40.0f, .soft = 70.0f};
    look.fogBehind.drift = {6.0f, 0.0f};
    look.fogFront.band = FogBand{.top = groundY - 70.0f, .bottom = groundY + 60.0f, .soft = 40.0f};
    look.fogFront.drift = {12.0f, 0.0f};
    return look;
}

Atmosphere::Atmosphere(const AtmosphereLook& look, std::uint64_t seed)
    : _ripples(look.ripples, seed)
    , _fogBehind(look.fogBehind, seed + 1U)
    , _dustBehind(look.dustBehind, seed + 2U)
    , _snowBehind(look.snowBehind, seed + 3U)
    , _rainBehind(look.rainBehind, seed + 4U)
    , _dustFront(look.dustFront, seed + 5U)
    , _snowFront(look.snowFront, seed + 6U)
    , _rainFront(look.rainFront, seed + 7U)
    , _fogFront(look.fogFront, seed + 8U) {}

void Atmosphere::Update(float seconds, const Weather& weather) {
    _weather = {.rain = Amount(weather.rain),
                .snow = Amount(weather.snow),
                .dust = Amount(weather.dust),
                .fog = Amount(weather.fog),
                .wind = weather.wind};
    _ripples.Update(seconds);
    _fogBehind.Update(seconds, _weather.wind);
    _dustBehind.Update(seconds, _weather.wind);
    _snowBehind.Update(seconds, _weather.wind);
    _rainBehind.Update(seconds, _weather.wind);
    _dustFront.Update(seconds, _weather.wind);
    _snowFront.Update(seconds, _weather.wind);
    _rainFront.Update(seconds, _weather.wind);
    _fogFront.Update(seconds, _weather.wind);
}

void Atmosphere::DrawBehind(Painter& paint, const CameraView& view) const {
    _ripples.Draw(paint, _weather.rain, view);
    _fogBehind.Draw(paint, _weather.fog, view);
    _dustBehind.Draw(paint, _weather.dust, view);
    _snowBehind.Draw(paint, _weather.snow, view);
    _rainBehind.Draw(paint, _weather.rain, view);
}

void Atmosphere::DrawFront(Painter& paint, const CameraView& view) const {
    _dustFront.Draw(paint, _weather.dust, view);
    _snowFront.Draw(paint, _weather.snow, view);
    _rainFront.Draw(paint, _weather.rain, view);
    _fogFront.Draw(paint, _weather.fog, view);
}
