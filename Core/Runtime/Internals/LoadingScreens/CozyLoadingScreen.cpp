#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawCozyLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0xf5e6d5);
    constexpr Color4 Ink = Rgb(0x79594c);
    constexpr Color4 Peach = Rgb(0xe9b39b);
    constexpr Color4 Sage = Rgb(0x91aa87);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        // Rounded clouds and hills stay inside the picture, leaving matching pastel margins.
        paint.RoundRect({19.0f, 41.0f, 73.0f, 19.0f}, 9.5f, Fill(Rgb(0xfbf2e6)));
        paint.Circle({45.0f, 39.0f}, 15.0f, Fill(Rgb(0xfbf2e6)));
        paint.Circle({66.0f, 43.0f}, 11.0f, Fill(Rgb(0xfbf2e6)));
        paint.RoundRect({234.0f, 61.0f, 64.0f, 17.0f}, 8.5f, Fill(Rgb(0xfbf2e6)));
        paint.Circle({257.0f, 59.0f}, 13.0f, Fill(Rgb(0xfbf2e6)));
        paint.Circle({277.0f, 62.0f}, 10.0f, Fill(Rgb(0xfbf2e6)));
        paint.Ellipse({64.0f, 143.0f}, {39.0f, 17.0f}, Fill(Rgb(0xe3d6bd)));
        paint.Ellipse({259.0f, 138.0f}, {34.0f, 22.0f}, Fill(Rgb(0xd6ddc4)));
        paint.Circle({160.0f, 48.0f}, 24.0f, Fill(Rgb(0xfbf2e6)));
        paint.Ellipse({160.0f, 64.0f}, {12.0f, 3.0f}, Fill(Rgb(0xe9dbc7)));
        const float sway = std::sin(frame.seconds * 1.1f) * 0.08f;
        {
            auto sprout = paint.Transform(Affine2::Rotate(sway, {160.0f, 63.0f}));
            paint.Line({160.0f, 62.0f}, {160.0f, 40.0f}, {.width = 2.0f, .color = Sage, .cap = StrokeCap::Round});
            paint.Ellipse({153.0f, 43.0f}, {8.0f, 4.5f}, Fill(Sage));
            paint.Ellipse({167.0f, 37.0f}, {8.0f, 4.5f}, Fill(Rgb(0xb4c49b)));
        }
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {160.0f - LoadingPixelTextWidth(Credit, 0.65f, 1.5f) * 0.5f, 84.0f}, 0.65f,
                             Ink, 1.5f);
        const float pixel = std::min(2.0f, 220.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {160.0f - LoadingPixelTextWidth(Name, pixel) * 0.5f, 99.0f}, pixel, Ink);
        for (int dot = 0; dot < 3; ++dot) {
            const float phase = frame.seconds * 2.4f - static_cast<float>(dot) * 0.8f;
            const float bounce = 0.5f + 0.5f * std::sin(phase);
            const float x = 149.0f + static_cast<float>(dot) * 11.0f;
            paint.Ellipse({x, 139.0f}, {3.0f - bounce * 0.6f, 0.8f}, Fill(Rgb(0xe5d2be)));
            paint.Circle({x, 134.0f - bounce * 5.0f}, 2.6f, Fill(dot == 1 ? Sage : Peach));
        }
        constexpr std::string_view Loading = "LOADING";
        DrawLoadingPixelText(paint, Loading, {160.0f - LoadingPixelTextWidth(Loading, 0.65f, 1.5f) * 0.5f, 150.0f},
                             0.65f, Ink, 1.5f);
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
