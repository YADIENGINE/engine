#pragma once
#include "Animation/PoseFields.h"
#include "Utilities/Geometry/Float2.h"

// What the weather is right now: plain values that draw nothing. It is a pose, so a tween turns one weather into
// another, a simulation can keep it in State and let rules read it, and Graphics/Atmosphere shows it:
//
//     SecondsTween<Weather> weather{Weather::Clear()};
//     weather.To(Weather::Storm(), 8.0f);
//     weather.Update(seconds);
//     air.Update(seconds, weather);
//
// Each amount is 0..1. A game's own kinds (ash, leaves) are its own values beside this one.
struct Weather {
    float rain = 0.0f;
    float snow = 0.0f;
    float dust = 0.0f;
    float fog = 0.0f;
    Float2 wind{}; // Canvas units per second

    [[nodiscard]] static constexpr Weather Clear() { return {}; }
    [[nodiscard]] static constexpr Weather Drizzle() { return {.rain = 0.3f, .fog = 0.2f, .wind = {-20.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather Rain() { return {.rain = 0.65f, .fog = 0.15f, .wind = {-60.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather Storm() { return {.rain = 1.0f, .fog = 0.35f, .wind = {-180.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather Snow() { return {.snow = 0.6f, .wind = {-15.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather Blizzard() { return {.snow = 1.0f, .fog = 0.5f, .wind = {-220.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather Mist() { return {.dust = 0.25f, .fog = 0.75f, .wind = {12.0f, 0.0f}}; }
    [[nodiscard]] static constexpr Weather StillAir() { return {.dust = 0.7f, .wind = {6.0f, 0.0f}}; }

    static constexpr auto Fields() {
        return PoseFields(PoseField("rain", &Weather::rain), PoseField("snow", &Weather::snow),
                          PoseField("dust", &Weather::dust), PoseField("fog", &Weather::fog),
                          PoseField("wind", &Weather::wind));
    }
    friend constexpr bool operator==(const Weather&, const Weather&) = default;
};

// What tweens and clips use to go between two weathers.
[[nodiscard]] inline Weather Mix(const Weather& from, const Weather& to, float t) { return MixPose(from, to, t); }
