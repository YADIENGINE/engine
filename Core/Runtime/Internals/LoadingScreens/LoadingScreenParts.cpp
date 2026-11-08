#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"

namespace {
// Each row uses its low five bits, with the leftmost pixel in bit four. The last glyph is space.
constexpr std::array<std::array<std::uint8_t, 7>, 27> PixelLetters{{
    {{14, 17, 17, 31, 17, 17, 17}}, // A
    {{30, 17, 17, 30, 17, 17, 30}}, // B
    {{14, 17, 16, 16, 16, 17, 14}}, // C
    {{30, 17, 17, 17, 17, 17, 30}}, // D
    {{31, 16, 16, 30, 16, 16, 31}}, // E
    {{31, 16, 16, 30, 16, 16, 16}}, // F
    {{14, 17, 16, 23, 17, 17, 15}}, // G
    {{17, 17, 17, 31, 17, 17, 17}}, // H
    {{31, 4, 4, 4, 4, 4, 31}},      // I
    {{7, 2, 2, 2, 18, 18, 12}},     // J
    {{17, 18, 20, 24, 20, 18, 17}}, // K
    {{16, 16, 16, 16, 16, 16, 31}}, // L
    {{17, 27, 21, 21, 17, 17, 17}}, // M
    {{17, 25, 21, 19, 17, 17, 17}}, // N
    {{14, 17, 17, 17, 17, 17, 14}}, // O
    {{30, 17, 17, 30, 16, 16, 16}}, // P
    {{14, 17, 17, 17, 21, 18, 13}}, // Q
    {{30, 17, 17, 30, 20, 18, 17}}, // R
    {{15, 16, 16, 14, 1, 1, 30}},   // S
    {{31, 4, 4, 4, 4, 4, 4}},       // T
    {{17, 17, 17, 17, 17, 17, 14}}, // U
    {{17, 17, 17, 17, 17, 10, 4}},  // V
    {{17, 17, 17, 21, 21, 21, 10}}, // W
    {{17, 17, 10, 4, 10, 17, 17}},  // X
    {{17, 17, 10, 4, 4, 4, 4}},     // Y
    {{31, 1, 2, 4, 8, 16, 31}},     // Z
    {{0, 0, 0, 0, 0, 0, 0}},        // Space
}};

constexpr float MarkWidth = 96.0f;
constexpr float MarkHeight = 8.0f;
constexpr float TrackWidth = 320.0f;
constexpr float SlideSeconds = 1.1f;
} // namespace

float LoadingPixelTextWidth(std::string_view text, float pixel, float spacing) {
    if (text.empty()) return 0.0f;
    return (static_cast<float>(text.size()) * (5.0f + spacing) - spacing) * pixel;
}

void DrawLoadingPixelText(Painter& paint, std::string_view text, Float2 position, float pixel, Color4 color,
                          float spacing) {
    for (const char letter : text) {
        const auto index = letter >= 'A' && letter <= 'Z' ? static_cast<std::size_t>(letter - 'A') : 26U;
        auto glyph = PixelLetters[index];
        for (std::size_t row = 0; row < glyph.size(); ++row) {
            int column = 0;
            while (column < 5) {
                if ((glyph[row] & (1U << (4 - column))) == 0U) {
                    ++column;
                    continue;
                }
                const int first = column;
                while (column < 5 && (glyph[row] & (1U << (4 - column))) != 0U)
                    ++column;
                const auto run = ((1U << (column - first)) - 1U) << (5 - column);
                std::size_t bottom = row + 1;
                while (bottom < glyph.size() && (glyph[bottom] & run) == run) {
                    glyph[bottom] = static_cast<std::uint8_t>(glyph[bottom] & ~run);
                    ++bottom;
                }
                paint.Rect({position.x + static_cast<float>(first) * pixel,
                            position.y + static_cast<float>(row) * pixel, static_cast<float>(column - first) * pixel,
                            static_cast<float>(bottom - row) * pixel},
                           Fill(color));
            }
        }
        position.x += (5.0f + spacing) * pixel;
    }
}

FloatRect LoadingScreenArea(Vec2 canvas) {
    const float scale =
        std::max(0.0f, std::min(static_cast<float>(canvas.x) / 320.0f, static_cast<float>(canvas.y) / 180.0f));
    const Float2 size{320.0f * scale, 180.0f * scale};
    return {(static_cast<float>(canvas.x) - size.x) * 0.5f, (static_cast<float>(canvas.y) - size.y) * 0.5f, size.x,
            size.y};
}

std::uint32_t LoadingScreenHash(std::uint32_t seed) {
    seed ^= seed >> 16U;
    seed *= 0x7feb352dU;
    seed ^= seed >> 15U;
    seed *= 0x846ca68bU;
    return seed ^ (seed >> 16U);
}

void DrawLoadingStandIn(Painter& paint, const LoadingScreenFrame& frame, Color4 background, Color4 mark) {
    const auto width = static_cast<float>(frame.canvas.x);
    const auto height = static_cast<float>(frame.canvas.y);
    paint.Rect({0.0f, 0.0f, width, height}, Fill(background));
    const float slide = std::abs(std::fmod(frame.seconds / SlideSeconds, 2.0f) - 1.0f);
    const float left = (width - TrackWidth) * 0.5f + slide * (TrackWidth - MarkWidth);
    paint.Rect({left, (height - MarkHeight) * 0.5f, MarkWidth, MarkHeight}, Fill(mark));
    paint.Rect({0.0f, 0.0f, width, height}, Fill(WithAlpha(Rgb(0x000000), 1.0f - frame.visibility)));
}
