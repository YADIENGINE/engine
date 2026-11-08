#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"

namespace {
void DrawFire(Painter& paint, Float2 at, float seconds, float phase, float scale) {
    auto local = paint.Transform(Affine2::Translate(at) * Affine2::Scale(scale));
    const float sway = std::sin(seconds * 2.7f + phase);
    const float lift = std::sin(seconds * 3.3f + phase) * 0.8f;
    // Only the small flame changes shape; the pool of light varies gently by four percent.
    paint.Glow({0.0f, -6.0f}, 32.0f,
               {.color = Rgb(0xd87b34), .strength = 0.36f + 0.014f * std::sin(seconds * 2.1f + phase)});
    const std::array<Float2, 7> flame{{{-4.0f, 0.0f},
                                       {-5.0f, -6.0f},
                                       {-2.0f, -12.0f},
                                       {sway * 1.8f, -19.0f + lift},
                                       {2.0f, -10.0f},
                                       {4.0f + sway, -6.0f},
                                       {3.0f, 0.0f}}};
    paint.Polygon(flame, Fill(Rgb(0xc36a2e)));
    paint.Triangle({-2.5f, -1.0f}, {sway, -13.0f + lift}, {2.5f, -1.0f}, Fill(Rgb(0xf5b75a)));
    paint.Triangle({-1.0f, -1.0f}, {-sway * 0.5f, -6.0f}, {1.5f, -1.0f}, Fill(Rgb(0xffdf9c)));
    for (std::uint32_t ember = 0; ember < 3U; ++ember) {
        const auto seed = LoadingScreenHash(ember + static_cast<std::uint32_t>(phase * 19.0f));
        const float travel = std::fmod(seconds * 0.31f + static_cast<float>(seed % 100U) * 0.01f, 1.0f);
        const float x = std::sin(travel * 5.0f + phase) * 3.0f;
        paint.Circle({x, -12.0f - travel * 19.0f}, 0.6f, Fill(WithAlpha(Rgb(0xeeb76c), std::sin(travel * Pi) * 0.7f)));
    }
}

} // namespace

void DrawRpgLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    constexpr Color4 Background = Rgb(0x121313);
    constexpr Color4 Mortar = Rgb(0x191b1b);
    constexpr Color4 Letter = Rgb(0xd7cbb4);
    constexpr std::string_view Name = ENGINE_NAME;
    const FloatRect cover =
        FloatRect{0.0f, 0.0f, static_cast<float>(frame.canvas.x), static_cast<float>(frame.canvas.y)}.Inset(
            -2.0f / paint.PixelScale());
    paint.Rect(cover, Fill(Background));
    const FloatRect area = LoadingScreenArea(frame.canvas);
    if (!area.Empty()) {
        auto clip = paint.Clip(area);
        auto layout = paint.Transform(Affine2::Translate(area.Position()) * Affine2::Scale(area.width / 320.0f));
        // Uneven courses surround a working gate, rather than framing the lettering.
        for (std::uint32_t row = 0; row < 6U; ++row) {
            for (std::uint32_t column = 0; column < 8U; ++column) {
                const auto seed = LoadingScreenHash(row * 8U + column + 47U);
                const float x = 10.0f + static_cast<float>(column) * 39.0f - (row % 2U == 0U ? 16.0f : 0.0f);
                const float y = 9.0f + static_cast<float>(row) * 22.0f;
                const float shade = static_cast<float>(seed % 100U) * 0.01f;
                const Color4 stone = Mix(Rgb(0x232727), Rgb(0x353735), shade);
                paint.Rect({std::max(x, 7.0f), y, std::min(x + 37.0f, 313.0f) - std::max(x, 7.0f), 20.5f},
                           Fill(x > 180.0f ? Mix(stone, Background, 0.55f) : stone));
            }
        }
        paint.Circle({102.0f, 67.0f}, 44.0f, Fill(Rgb(0x211b16)));
        paint.Rect({58.0f, 67.0f, 88.0f, 76.0f}, Fill(Rgb(0x211b16)));
        for (std::uint32_t plank = 0; plank < 10U; ++plank) {
            const float x = 59.0f + static_cast<float>(plank) * 8.6f;
            const float edge = std::max(std::abs(x - 102.0f), std::abs(x + 7.7f - 102.0f));
            const float top = 67.0f - std::sqrt(std::max(0.0f, 43.0f * 43.0f - edge * edge));
            const auto seed = LoadingScreenHash(plank + 74U);
            paint.Rect({x, top, 7.7f, 143.0f - top},
                       Fill(Mix(Rgb(0x33261c), Rgb(0x4e3927), static_cast<float>(seed % 100U) * 0.01f)));
            paint.Line({x + 2.1f, top + 5.0f}, {x + 2.8f, 139.0f}, {.width = 0.5f, .color = Rgb(0x261e18)});
        }
        for (const float y : {93.0f, 130.0f}) {
            paint.Rect({60.0f, y, 84.0f, 4.0f}, Fill(Rgb(0x171b1b)));
            for (const float x : {64.0f, 78.0f, 126.0f, 140.0f})
                paint.Circle({x, y + 2.0f}, 0.8f, Fill(Rgb(0x727069)));
        }
        for (int block = 0; block < 11; ++block) {
            const float start = Pi + static_cast<float>(block) * Pi / 11.0f + 0.012f;
            const float end = Pi + static_cast<float>(block + 1) * Pi / 11.0f - 0.012f;
            const Float2 a{std::cos(start), std::sin(start)};
            const Float2 b{std::cos(end), std::sin(end)};
            const Float2 center{102.0f, 67.0f};
            const std::array<Float2, 4> stone{
                {center + a * 44.0f, center + a * 57.0f, center + b * 57.0f, center + b * 44.0f}};
            paint.Polygon(stone, {.fill = block == 5 ? Rgb(0x656259) : Rgb(0x4b4e49), .stroke = {0.8f, Mortar}});
        }
        for (const float x : {45.0f, 146.0f}) {
            for (int block = 0; block < 4; ++block) {
                paint.Rect({x, 68.0f + static_cast<float>(block) * 19.0f, 13.0f, 17.5f},
                           Fill(block % 2 == 0 ? Rgb(0x494b46) : Rgb(0x3c403c)));
            }
        }
        for (const float x : {34.0f, 171.0f}) {
            paint.Rect({x - 3.0f, 92.0f, 6.0f, 14.0f}, Fill(Rgb(0x171a1a)));
            paint.Line({x, 100.0f}, {x, 84.0f}, {.width = 3.0f, .color = Rgb(0x67452c)});
            DrawFire(paint, {x, 83.0f}, frame.seconds, x * 0.1f, 1.0f);
            const std::array<Float2, 4> basket{
                {{x - 6.0f, 82.0f}, {x + 6.0f, 82.0f}, {x + 3.0f, 88.0f}, {x - 3.0f, 88.0f}}};
            paint.Polygon(basket, {.fill = Rgb(0x252a28), .stroke = {0.8f, Rgb(0x777267)}});
        }
        paint.Rect({0.0f, 144.0f, 320.0f, 36.0f}, {.gradient = Gradient::Vertical(36.0f, Rgb(0x1d1e1c), Background)});
        paint.Rect({47.0f, 142.0f, 111.0f, 4.0f}, Fill(Rgb(0x49483e)));
        paint.Rect({40.0f, 146.0f, 125.0f, 4.0f}, Fill(Rgb(0x2c2f2b)));
        for (const float x : {60.0f, 142.0f, 245.0f})
            paint.Line({x, 153.0f}, {x - 22.0f, 174.0f}, {.width = 0.6f, .color = Rgb(0x292b27)});
        // Soft edges meet the same dark stone colour on portrait and square canvases.
        paint.Rect({0.0f, 0.0f, 320.0f, 18.0f},
                   {.gradient = Gradient::Vertical(18.0f, Background, WithAlpha(Background, 0.0f))});
        paint.Rect({0.0f, 0.0f, 24.0f, 180.0f}, {.gradient = Gradient::Linear({-12.0f, 0.0f}, {12.0f, 0.0f}, Background,
                                                                              WithAlpha(Background, 0.0f))});
        paint.Rect(
            {296.0f, 0.0f, 24.0f, 180.0f},
            {.gradient = Gradient::Linear({-12.0f, 0.0f}, {12.0f, 0.0f}, WithAlpha(Background, 0.0f), Background)});
        constexpr std::string_view Credit = "MADE WITH";
        DrawLoadingPixelText(paint, Credit, {242.0f - LoadingPixelTextWidth(Credit, 0.85f, 1.5f) * 0.5f, 67.0f}, 0.85f,
                             Rgb(0xa69f8d), 1.5f);
        const float pixel = std::min(2.7f, 118.0f / std::max(LoadingPixelTextWidth(Name, 1.0f), 1.0f));
        DrawLoadingPixelText(paint, Name, {242.0f - LoadingPixelTextWidth(Name, pixel) * 0.5f, 84.0f}, pixel, Letter);
        constexpr std::string_view Loading = "LOADING";
        const float loadingWidth = LoadingPixelTextWidth(Loading, 0.85f, 1.3f);
        const float loadingX = 242.0f - (loadingWidth + 15.0f) * 0.5f;
        // A miniature brazier is the loading indicator. Its fire never waits for load progress.
        DrawFire(paint, {loadingX + 4.0f, 123.0f}, frame.seconds, 7.0f, 0.45f);
        paint.Line({loadingX + 1.0f, 124.0f}, {loadingX + 7.0f, 124.0f}, {.width = 2.0f, .color = Rgb(0x79766a)});
        DrawLoadingPixelText(paint, Loading, {loadingX + 15.0f, 119.0f}, 0.85f, Rgb(0xc3b299), 1.3f);
    }
    paint.Rect(cover, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
