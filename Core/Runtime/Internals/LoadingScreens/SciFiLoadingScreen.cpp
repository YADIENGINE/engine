#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

namespace {
constexpr Float2 PlanetCenter{232.0f, 78.0f};

Float2 OrbitPoint(float angle, float minorRadius = 24.0f) {
    const float x = std::cos(angle) * 76.0f;
    const float y = std::sin(angle) * minorRadius;
    return PlanetCenter + Float2{x * 0.94f + y * 0.342f, -x * 0.342f + y * 0.94f};
}

void DrawRing(Painter& paint, float start, Color4 color) {
    std::array<Float2, 41> points{};
    for (std::size_t i = 0; i < points.size(); ++i)
        points[i] = OrbitPoint(start + static_cast<float>(i) * Pi / static_cast<float>(points.size() - 1U));
    paint.Polyline(points, {.width = 4.0f, .color = WithAlpha(color, 0.22f)});
    paint.Polyline(points, {.width = 1.6f, .color = WithAlpha(color, 0.65f)});
    paint.Polyline(points, {.width = 0.4f, .color = color});
}

void DrawSatellite(Painter& paint, float angle, float seconds) {
    // A wider inclined orbit keeps the loading indicator visible around the entire planetary limb.
    const Float2 at = OrbitPoint(angle, 65.0f);
    auto local = paint.Transform(Affine2::Translate(at) * Affine2::Rotate(-0.35f + std::sin(angle) * 0.2f));
    paint.Line({-8.0f, 0.0f}, {8.0f, 0.0f}, {.width = 0.7f, .color = Rgb(0xb6c5ca)});
    paint.Rect({-8.0f, -2.3f, 5.0f, 4.6f}, {.fill = Rgb(0x344e6a), .stroke = {0.5f, Rgb(0x8db1c6)}});
    paint.Rect({3.0f, -2.3f, 5.0f, 4.6f}, {.fill = Rgb(0x344e6a), .stroke = {0.5f, Rgb(0x8db1c6)}});
    paint.Rect({-1.6f, -2.5f, 3.2f, 5.0f}, Fill(Rgb(0xd5d5c7)));
    paint.Line({0.0f, -2.5f}, {0.0f, -5.0f}, {.width = 0.6f, .color = Rgb(0xb6c5ca)});
    // A steady thruster has a small local flutter, without a brightness pulse.
    paint.Triangle({-0.8f, 2.5f}, {0.8f, 2.5f}, {std::sin(seconds * 4.0f) * 0.3f, 5.5f}, Fill(Rgb(0x8fd6e2)));
}
} // namespace

void DrawSciFiLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x070c16);
    constexpr Color4 Letter = Rgb(0xd7e6eb);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto clip = paint.Clip(area);
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        paint.Glow({218.0f, 79.0f}, 93.0f, {.color = Rgb(0x334e73), .strength = 0.16f});
        paint.Glow({96.0f, 142.0f}, 58.0f, {.color = Rgb(0x493951), .strength = 0.1f});
        // Three quiet star layers cross the picture at different speeds, even during a long load.
        for (std::uint32_t star = 0; star < 68U; ++star) {
            const auto seed = LoadingScreenHash(star + 311U);
            const float depth = static_cast<float>(star % 3U);
            const float x =
                std::fmod(static_cast<float>(seed % 3200U) * 0.1f + frame.seconds * (0.35f + depth * 0.7f), 320.0f);
            const float y = 5.0f + static_cast<float>((seed >> 12U) % 1700U) * 0.1f;
            const float edge = std::min({x / 12.0f, (320.0f - x) / 12.0f, y / 12.0f, (180.0f - y) / 12.0f, 1.0f});
            // Leave the lettering's immediate neighbourhood clear of foreground stars.
            const float quiet = x > 22.0f && x < 155.0f && y > 55.0f && y < 130.0f ? 0.22f : 1.0f;
            const Color4 color = Mix(Rgb(0x64738d), Rgb(0xd2dedc), depth * 0.4f);
            paint.Circle({x, y}, 0.3f + depth * 0.18f, Fill(WithAlpha(color, edge * quiet)));
        }
        paint.Circle({286.0f, 25.0f}, 7.5f,
                     {.gradient = Gradient::Linear({-5.0f, -5.0f}, {5.0f, 4.0f}, Rgb(0x8e999e), Rgb(0x192130))});
        paint.Circle({283.0f, 24.0f}, 1.8f, Fill(Rgb(0x5a6873)));
        paint.Circle({286.0f, 21.0f}, 0.9f, Fill(Rgb(0x65727b)));
        const float orbit = frame.seconds * 0.55f + 0.8f;
        DrawRing(paint, Pi, Rgb(0x667d93));
        // The atmosphere and surface share a fixed sun direction at the upper left.
        paint.Circle(PlanetCenter, 53.8f,
                     {.gradient = Gradient::Linear({-39.0f, -34.0f}, {35.0f, 33.0f}, Rgb(0x93c7d9), Background)});
        paint.Circle(PlanetCenter, 52.0f,
                     {.gradient = Gradient::Linear({-38.0f, -38.0f}, {36.0f, 29.0f}, Rgb(0x7996a1), Rgb(0x152333))});
        // Curved cloud belts are bounded by the sphere, without textures or per-pixel work.
        for (std::uint32_t belt = 0; belt < 9U; ++belt) {
            const auto seed = LoadingScreenHash(belt + 811U);
            const float y = -39.0f + static_cast<float>(belt) * 9.0f;
            const float halfWidth = std::sqrt(51.0f * 51.0f - y * y) * 0.95f;
            const float thickness = 1.5f + static_cast<float>(seed % 4U) * 0.5f;
            std::array<Float2, 26> points{};
            for (std::size_t i = 0; i < points.size() / 2U; ++i) {
                const float u = static_cast<float>(i) / 12.0f;
                const float x = (u * 2.0f - 1.0f) * halfWidth;
                const float curve = std::sin(u * Pi) * (4.0f + y * 0.08f);
                const float ripple = std::sin(u * 11.0f + static_cast<float>(belt)) * 0.6f;
                points[i] = PlanetCenter + Float2{x, y + curve + ripple};
                points[25U - i] = PlanetCenter + Float2{x, y + curve + ripple + thickness * std::sin(u * Pi)};
            }
            paint.Polygon(points, Fill(WithAlpha(belt % 2U == 0U ? Rgb(0xc1c5b1) : Rgb(0x37536c), 0.42f)));
        }
        paint.Circle(PlanetCenter, 52.0f,
                     {.gradient = Gradient::Linear({-22.0f, -22.0f}, {27.0f, 16.0f}, WithAlpha(Background, 0.0f),
                                                   WithAlpha(Background, 0.95f))});
        paint.Arc(PlanetCenter, 53.5f, Pi * 0.93f, Pi * 0.68f, {.width = 0.65f, .color = Rgb(0x9bc6d6)});
        DrawRing(paint, 0.0f, Rgb(0xa6a99e));
        DrawSatellite(paint, orbit, frame.seconds);
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {28.0f, 65.0f}, 0.85f, Rgb(0x8a9faf), 1.6f);
        const float pixel = std::min(2.7f, 129.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {28.0f, 82.0f}, pixel, Letter);
        constexpr std::string_view Loading = "LOADING";
        DrawLoadingPixelText(paint, Loading, {28.0f, 118.0f}, 0.85f, Rgb(0xa8c2ce), 1.3f);
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
