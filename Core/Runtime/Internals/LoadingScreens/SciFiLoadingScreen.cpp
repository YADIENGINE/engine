#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawSciFiLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x071321);
    constexpr Color4 Cyan = Rgb(0x72ddf5);
    constexpr Color4 Muted = Rgb(0x3b718b);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        for (int column = 0; column <= 16; ++column) {
            const float x = static_cast<float>(column) * 20.0f;
            paint.Line({x, 10.0f}, {x, 170.0f}, {.width = 0.3f, .color = Rgb(0x142b3d)});
        }
        for (int row = 1; row < 9; ++row) {
            const float y = static_cast<float>(row) * 20.0f;
            paint.Line({10.0f, y}, {310.0f, y}, {.width = 0.3f, .color = Rgb(0x142b3d)});
        }
        // A single fine sweep is a HUD cursor, with no tracking band or brightness pulse.
        const float scan = 20.0f + std::fmod(frame.seconds * 37.0f, 280.0f);
        paint.Line({scan, 24.0f}, {scan, 156.0f}, {.width = 0.6f, .color = Rgb(0x27617b)});
        paint.Rect({scan - 1.5f, 21.0f, 3.0f, 3.0f}, Fill(Cyan));
        paint.Rect({scan - 1.5f, 156.0f, 3.0f, 3.0f}, Fill(Cyan));
        for (const float x : {18.0f, 302.0f}) {
            for (const float y : {18.0f, 162.0f}) {
                const float dx = x < 160.0f ? 1.0f : -1.0f;
                const float dy = y < 90.0f ? 1.0f : -1.0f;
                paint.Line({x, y + dy * 10.0f}, {x, y}, {.width = 1.0f, .color = Cyan});
                paint.Line({x, y}, {x + dx * 14.0f, y}, {.width = 1.0f, .color = Cyan});
            }
        }
        paint.Rect({32.0f, 55.0f, 2.0f, 64.0f}, Fill(Cyan));
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {43.0f, 59.0f}, 0.75f, Muted, 1.5f);
        const float pixel = std::min(2.0f, 158.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {43.0f, 77.0f}, pixel, Cyan);
        paint.Line({43.0f, 104.0f}, {182.0f, 104.0f}, {.width = 0.5f, .color = Muted});
        constexpr std::string_view Loading = "LOADING";
        DrawLoadingPixelText(paint, Loading, {43.0f, 112.0f}, 0.8f, Cyan);
        const float angle = frame.seconds * 1.8f;
        paint.Arc({246.0f, 86.0f}, 30.0f, 0.0f, 2.0f * Pi, {.width = 0.4f, .color = Muted});
        paint.Arc({246.0f, 86.0f}, 23.0f, angle, Pi * 1.25f, {.width = 1.6f, .color = Cyan});
        paint.Arc({246.0f, 86.0f}, 17.0f, -angle * 0.7f, Pi * 0.65f, {.width = 0.8f, .color = Muted});
        paint.Line({241.0f, 86.0f}, {251.0f, 86.0f}, {.width = 0.7f, .color = Cyan});
        paint.Line({246.0f, 81.0f}, {246.0f, 91.0f}, {.width = 0.7f, .color = Cyan});
        for (int tick = 0; tick < 12; ++tick) {
            const float a = static_cast<float>(tick) * Pi / 6.0f;
            const Float2 direction{std::cos(a), std::sin(a)};
            paint.Line(Float2{246.0f, 86.0f} + direction * 33.0f, Float2{246.0f, 86.0f} + direction * 36.0f,
                       {.width = 0.6f, .color = Muted});
        }
        for (std::uint32_t bit = 0; bit < 14U; ++bit) {
            const float x = 43.0f + static_cast<float>(bit) * 6.0f;
            const float height = 2.0f + static_cast<float>(LoadingScreenHash(bit + 17U) % 7U);
            paint.Rect({x, 145.0f - height, 2.0f, height}, Fill(Muted));
        }
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
