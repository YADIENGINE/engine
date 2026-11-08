#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawDefaultLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x0f0e0d);
    constexpr Color4 Amber = Rgb(0xf4af65);
    constexpr Color4 Ink = Rgb(0x3a2c20);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect canvas{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)};
    // Overscan keeps antialiasing at the canvas corners outside the visible image, including the black endpoint.
    const FloatRect cover = canvas.Inset(-2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        paint.Rect({142.0f, 43.0f, 36.0f, 36.0f}, Fill(Amber));
        DrawLoadingPixelText(paint, Name.substr(0, 1), {152.5f, 50.5f}, 3.0f, Ink);
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {160.0f - LoadingPixelTextWidth(Credit, 0.65f, 2.0f) * 0.5f, 92.0f}, 0.65f,
                             Amber, 2.0f);
        const float namePixel = std::min(1.75f, 250.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {160.0f - LoadingPixelTextWidth(Name, namePixel) * 0.5f, 105.0f}, namePixel,
                             Amber);

        constexpr std::string_view Loading = "LOADING";
        const float labelWidth = LoadingPixelTextWidth(Loading, 0.65f);
        const float left = 160.0f - (labelWidth + 13.0f) * 0.5f;
        DrawLoadingPixelText(paint, Loading, {left + 13.0f, 155.0f}, 0.65f, Amber);
        // Three points orbit at constant speed for as long as the screen is open.
        for (int point = 0; point < 3; ++point) {
            const float angle = frame.seconds * 3.0f - static_cast<float>(point) * 0.8f;
            paint.Rect({left + 3.0f + std::cos(angle) * 3.0f, 157.0f + std::sin(angle) * 3.0f, 1.2f, 1.2f},
                       Fill(WithAlpha(Amber, 1.0f - static_cast<float>(point) * 0.25f)));
        }
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
