#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Surface/Surface.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

void DrawHorrorLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x050907);
    constexpr Color4 Foreground = Rgb(0xbac9bf);
    constexpr Color4 Secondary = Rgb(0x64786b);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect canvas{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)};
    const FloatRect area = LoadingScreenArea(frame.canvas);
    const float visibility = std::clamp(frame.visibility, 0.0f, 1.0f);
    const FloatRect cover = canvas.Inset(-2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    if (!area.Empty()) {
        // Surface sizes follow device scale; compensate to keep exactly 320x180 device pixels.
        const float deviceScale = paint.PixelScale();
        const Float2 surfaceSize = ToFloat2(LoadingScreenPixelSize) / deviceScale;
        if (frame.pixels.Size() != surfaceSize) frame.pixels.Resize(surfaceSize);
        const auto tick = static_cast<std::uint32_t>(std::fmod(std::floor(frame.seconds * 24.0f), 1048576.0f));
        const std::uint32_t signal = LoadingScreenHash(tick);
        const float namePixel = std::min(3.0f, 250.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        const float nameWidth = LoadingPixelTextWidth(Name, namePixel);
        const float nameLeft = std::floor(160.0f - nameWidth * 0.5f);
        {
            auto into = paint.Target(frame.pixels, Background);
            auto pixels = paint.Transform(Affine2::Scale(1.0f / deviceScale));
            paint.Rect({13.0f, 11.0f, 294.0f, 158.0f}, Outline(1.0f, Rgb(0x17231c), StrokeAlign::Inside));
            paint.Rect({17.0f, 15.0f, 286.0f, 150.0f}, Outline(1.0f, Rgb(0x0c1510), StrokeAlign::Inside));
            paint.Rect({23.0f, 22.0f, 10.0f, 1.0f}, Fill(Secondary));
            paint.Rect({23.0f, 22.0f, 1.0f, 5.0f}, Fill(Secondary));
            paint.Rect({287.0f, 157.0f, 10.0f, 1.0f}, Fill(Secondary));
            paint.Rect({296.0f, 153.0f, 1.0f, 5.0f}, Fill(Secondary));
            {
                // Only the lettering trembles; brightness varies by at most six percent.
                const float tremble = signal % 8U == 0U ? static_cast<float>(static_cast<int>(signal % 3U) - 1) : 0.0f;
                auto text = paint.Transform(Affine2::Translate({tremble, 0.0f}));
                auto flicker = paint.Opacity(0.94f + static_cast<float>(signal % 7U) * 0.01f);
                constexpr std::string_view Credit = "MADE WITH";
                DrawLoadingPixelText(paint, Credit,
                                     {std::floor(160.0f - LoadingPixelTextWidth(Credit, 1.0f) * 0.5f), 62.0f}, 1.0f,
                                     Secondary);
                DrawLoadingPixelText(paint, Name, {nameLeft, 80.0f}, namePixel, Foreground);
                constexpr std::string_view Loading = "LOADING";
                DrawLoadingPixelText(paint, Loading,
                                     {std::floor(160.0f - LoadingPixelTextWidth(Loading, 1.0f) * 0.5f), 143.0f}, 1.0f,
                                     Secondary);
            }
            {
                // A circulating dash, with a short dim trail, never measures completion.
                const float travel = std::fmod(frame.seconds * 29.0f, 58.0f);
                auto track = paint.Clip({131.0f, 128.0f, 58.0f, 2.0f});
                for (int copy = -1; copy <= 1; ++copy) {
                    const float x = 131.0f + travel + static_cast<float>(copy) * 58.0f;
                    paint.Rect({x - 6.0f, 128.0f, 6.0f, 1.0f}, Fill(WithAlpha(Secondary, 0.35f)));
                    paint.Rect({x, 128.0f, 7.0f, 1.0f}, Fill(Foreground));
                }
            }
            for (std::uint32_t point = 0; point < 22U; ++point) {
                const std::uint32_t noise = LoadingScreenHash(tick * 43U + point);
                paint.Rect({static_cast<float>(noise % 280U) + 20.0f, static_cast<float>((noise >> 10U) % 140U) + 20.0f,
                            1.0f, 1.0f},
                           Fill(WithAlpha(Secondary, 0.12f + static_cast<float>(noise % 4U) * 0.04f)));
            }
            const float bandY = std::floor(std::fmod(frame.seconds * 13.0f + 34.0f, 200.0f)) - 20.0f;
            auto glass = paint.Clip({18.0f, 16.0f, 284.0f, 148.0f});
            paint.Rect({18.0f, bandY, 284.0f, 12.0f}, Fill(Rgb(0x000000, 65)));
            paint.Rect({18.0f, bandY + 12.0f, 284.0f, 3.0f}, Fill(Rgb(0x000000, 30)));
        }
        {
            auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
            paint.Image(frame.pixels, {0.0f, 0.0f, 320.0f, 180.0f});
            const float phase = std::fmod(frame.seconds, 1.7f);
            if (phase > 0.72f && phase < 0.81f) {
                // A two-pixel slice through the title slips sideways for a few frames.
                const float sliceY = 80.0f + static_cast<float>(LoadingScreenHash(tick / 2U) % 17U);
                const FloatRect slice{nameLeft - 3.0f, sliceY, nameWidth + 6.0f, 2.0f};
                paint.Rect(slice, Fill(Background));
                const float shift = (signal & 1U) == 0U ? -2.0f : 2.0f;
                paint.Image(frame.pixels, slice.Translated({shift, 0.0f}),
                            {.source = {slice.x / deviceScale, slice.y / deviceScale, slice.width / deviceScale,
                                        slice.height / deviceScale}});
            }
        }
        auto& post = paint.Post();
        post.region = area;
        post.vignette = 0.22f * visibility;
        post.scanlines = 0.12f * visibility;
        post.scanlineCount = 180.0f;
        post.grain = {.amount = 0.035f * visibility, .size = area.width / 320.0f};
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - visibility)));
}
