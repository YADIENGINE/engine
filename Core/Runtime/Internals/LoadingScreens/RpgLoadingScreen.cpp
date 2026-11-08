#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawRpgLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x100e18);
    constexpr Color4 Gold = Rgb(0xd7b66e);
    constexpr Color4 Muted = Rgb(0x756044);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        const float breath = 0.5f + 0.5f * std::sin(frame.seconds * 0.9f);
        paint.Glow({160.0f, 75.0f}, 77.0f, {.color = Rgb(0xa06038), .strength = 0.12f + breath * 0.06f});
        paint.Rect({24.0f, 18.0f, 272.0f, 144.0f}, Outline(0.6f, Muted));
        paint.Rect({28.0f, 22.0f, 264.0f, 136.0f}, Outline(0.3f, Rgb(0x3e3034)));
        for (const float x : {24.0f, 296.0f}) {
            for (const float y : {18.0f, 162.0f}) {
                const float dx = x < 160.0f ? 1.0f : -1.0f;
                const float dy = y < 90.0f ? 1.0f : -1.0f;
                const std::array<Float2, 4> flourish{
                    {{x, y + dy * 19.0f}, {x + dx * 7.0f, y + dy * 7.0f}, {x + dx * 19.0f, y}, {x, y}}};
                paint.Polyline(flourish, {.width = 0.9f, .color = Gold});
                paint.Circle({x + dx * 8.0f, y + dy * 8.0f}, 1.2f, Fill(Gold));
            }
        }
        // A faceted seal, lit slowly from within.
        paint.Arc({160.0f, 52.0f}, 21.0f, 0.0f, 2.0f * Pi, {.width = 0.5f, .color = Muted});
        const std::array<Float2, 4> seal{{{160.0f, 29.0f}, {174.0f, 52.0f}, {160.0f, 75.0f}, {146.0f, 52.0f}}};
        paint.Polygon(seal, {.fill = Rgb(0x342337), .stroke = {0.8f, Gold}});
        paint.Triangle({160.0f, 34.0f}, {160.0f, 68.0f}, {150.0f, 52.0f}, Fill(Rgb(0x795349)));
        paint.Triangle({160.0f, 34.0f}, {170.0f, 52.0f}, {160.0f, 68.0f},
                       Fill(Mix(Rgb(0x9d784f), Gold, breath * 0.35f)));
        for (std::uint32_t spark = 0; spark < 16U; ++spark) {
            const std::uint32_t seed = LoadingScreenHash(spark + 91U);
            const float phase = std::fmod(frame.seconds * (0.075f + static_cast<float>(seed % 5U) * 0.008f) +
                                              static_cast<float>(seed % 1000U) / 1000.0f,
                                          1.0f);
            const float x = 43.0f + static_cast<float>((seed >> 8U) % 234U) + std::sin(phase * Pi * 2.0f) * 4.0f;
            paint.Circle({x, 149.0f - phase * 115.0f}, 0.45f + static_cast<float>(seed % 3U) * 0.2f,
                         Fill(WithAlpha(Rgb(0xeea45f), std::sin(phase * Pi) * 0.65f)));
        }
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {160.0f - LoadingPixelTextWidth(Credit, 0.65f, 2.0f) * 0.5f, 85.0f}, 0.65f,
                             Muted, 2.0f);
        const float pixel = std::min(2.25f, 230.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {160.0f - LoadingPixelTextWidth(Name, pixel) * 0.5f, 99.0f}, pixel, Gold);
        paint.Line({106.0f, 126.0f}, {149.0f, 126.0f}, {.width = 0.5f, .color = Muted});
        paint.Line({171.0f, 126.0f}, {214.0f, 126.0f}, {.width = 0.5f, .color = Muted});
        {
            auto turn = paint.Transform(Affine2::Rotate(frame.seconds * 0.8f, {160.0f, 126.0f}));
            paint.Rect({157.0f, 123.0f, 6.0f, 6.0f}, Outline(0.8f, Gold));
        }
        constexpr std::string_view Loading = "LOADING";
        DrawLoadingPixelText(paint, Loading, {160.0f - LoadingPixelTextWidth(Loading, 0.7f, 1.8f) * 0.5f, 141.0f}, 0.7f,
                             Gold, 1.8f);
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
