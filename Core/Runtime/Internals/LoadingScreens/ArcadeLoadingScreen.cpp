#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Surface/Surface.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawArcadeLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x211039);
    constexpr Color4 Cream = Rgb(0xffe9a3);
    constexpr std::array<Color4, 3> Colors{Rgb(0xff639a), Rgb(0x55ddd0), Rgb(0xffcc58)};
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        const float deviceScale = paint.PixelScale();
        const Float2 surfaceSize = ToFloat2(LoadingScreenPixelSize) / deviceScale;
        if (frame.pixels.Size() != surfaceSize) frame.pixels.Resize(surfaceSize);
        const float step = std::floor(frame.seconds * 8.0f);
        {
            auto into = paint.Target(frame.pixels, Background);
            auto pixels = paint.Transform(Affine2::Scale(1.0f / deviceScale));
            // Fixed colour stripes frame the cabinet; nothing flashes or changes palette.
            for (int stripe = 0; stripe < 3; ++stripe) {
                const float y = 17.0f + static_cast<float>(stripe) * 4.0f;
                paint.Rect({20.0f, y, 280.0f, 2.0f}, Fill(Colors[static_cast<std::size_t>(stripe)]));
                paint.Rect({20.0f, 178.0f - y, 280.0f, 2.0f}, Fill(Colors[static_cast<std::size_t>(stripe)]));
            }
            constexpr std::string_view Credit = "MADE WITH";
            DrawLoadingPixelText(paint, Credit,
                                 {std::floor(160.0f - LoadingPixelTextWidth(Credit, 1.0f) * 0.5f), 42.0f}, 1.0f,
                                 Colors[1]);
            const float pixel = std::min(3.0f, 252.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
            DrawLoadingPixelText(paint, Name, {std::floor(160.0f - LoadingPixelTextWidth(Name, pixel) * 0.5f), 61.0f},
                                 pixel, Cream);
            // Five block tokens hop in eight discrete poses, indefinitely.
            constexpr std::array<float, 8> Hop{0.0f, 3.0f, 6.0f, 8.0f, 9.0f, 8.0f, 6.0f, 3.0f};
            for (int token = 0; token < 5; ++token) {
                const float x = 118.0f + static_cast<float>(token) * 19.0f;
                const auto pose = static_cast<std::size_t>(std::fmod(step + static_cast<float>(token) * 2.0f, 8.0f));
                const float y = 115.0f - Hop[pose];
                const Color4 color = Colors[static_cast<std::size_t>(token) % Colors.size()];
                paint.Rect({x, 127.0f, 9.0f, 2.0f}, Fill(Rgb(0x38214f)));
                paint.Rect({x + 2.0f, y, 5.0f, 9.0f}, Fill(color));
                paint.Rect({x, y + 2.0f, 9.0f, 5.0f}, Fill(color));
                paint.Rect({x + 2.0f, y + 2.0f, 2.0f, 2.0f}, Fill(Cream));
            }
            constexpr std::string_view Loading = "LOADING";
            DrawLoadingPixelText(paint, Loading,
                                 {std::floor(160.0f - LoadingPixelTextWidth(Loading, 1.0f) * 0.5f), 139.0f}, 1.0f,
                                 Colors[0]);
            for (int row = 0; row < 45; ++row)
                paint.Rect({0.0f, static_cast<float>(row) * 4.0f, 320.0f, 1.0f}, Fill(Rgb(0x08041a, 35)));
        }
        paint.Image(frame.pixels, area);
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
