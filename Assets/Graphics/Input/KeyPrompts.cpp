#include "Graphics/Input/KeyPrompts.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr float GlyphWidth = 4.0f; // Grid cells of a stroke letter
constexpr float GlyphHeight = 6.0f;
constexpr float GlyphGap = 1.7f; // Between letters of a word
constexpr float StrokeOfCell = 0.95f;
constexpr float LabelShare = 0.5f;  // A label's height, of the key's top
constexpr float LabelMargin = 0.3f; // Free space at each side of a label, of the key's top
constexpr float EdgeShare = 0.13f;  // A key's front edge, of its height
constexpr float PressDepth = 0.75f; // How much of the edge a full press uses
constexpr float CornerShare = 0.2f; // A key's corners, of its height
constexpr float ClusterGap = 0.08f; // Between the keys of a set, of a key
constexpr float ClusterAspect = (3.0f + 2.0f * ClusterGap) / (2.0f + ClusterGap);
constexpr float MouseAspect = 0.68f;
constexpr float MouseButtons = 0.44f; // How far the buttons reach down the body
constexpr std::size_t MouseArcPoints = 12;

// Stroke letters on a 5 by 7 grid: pairs of digits are x y, '|' lifts the pen.
constexpr auto Letters = std::to_array<std::string_view>({
    "06 01 10 30 41 46|04 44",                     // A
    "06 00 30 41 42 33 44 45 36 06|03 33",         // B
    "41 30 10 01 05 16 36 45",                     // C
    "00 30 41 45 36 06 00",                        // D
    "40 00 06 46|03 33",                           // E
    "40 00 06|03 33",                              // F
    "41 30 10 01 05 16 36 45 43 23",               // G
    "00 06|40 46|03 43",                           // H
    "10 30|20 26|16 36",                           // I
    "20 40 45 36 16 05",                           // J
    "00 06|40 03 46",                              // K
    "00 06 46",                                    // L
    "06 00 23 40 46",                              // M
    "06 00 46 40",                                 // N
    "10 30 41 45 36 16 05 01 10",                  // O
    "06 00 30 41 42 33 03",                        // P
    "10 30 41 45 36 16 05 01 10|24 46",            // Q
    "06 00 30 41 42 33 03|23 46",                  // R
    "41 30 10 01 02 13 33 44 45 36 16 05",         // S
    "00 40|20 26",                                 // T
    "00 05 16 36 45 40",                           // U
    "00 26 40",                                    // V
    "00 16 23 36 40",                              // W
    "00 46|40 06",                                 // X
    "00 23 40|23 26",                              // Y
    "00 40 06 46",                                 // Z
});
constexpr auto Digits = std::to_array<std::string_view>({
    "10 30 41 45 36 16 05 01 10|14 32",            // 0
    "11 20 26|16 36",                              // 1
    "01 10 30 41 42 06 46",                        // 2
    "01 10 30 41 42 33 44 45 36 16 05|13 33",      // 3
    "36 30 04 44",                                 // 4
    "40 00 03 33 44 45 36 16 05",                  // 5
    "41 30 10 01 05 16 36 45 44 33 03",            // 6
    "00 40 16",                                    // 7
    "13 02 01 10 30 41 42 33 13 04 05 16 36 45 44 33", // 8
    "05 16 36 45 41 30 10 01 02 13 43",            // 9
});
static_assert(Letters.size() == 26 && Digits.size() == 10);

struct KeyRow {
    std::string_view label; // Empty: an arrow key, or not a single key
    float aspect = 1.0f;
};

// clang-format off
// In KeyPrompt's order. Words are wider keys, as on a keyboard, so they stay readable.
constexpr auto Keys = std::to_array<KeyRow>({
    {"A"}, {"B"}, {"C"}, {"D"}, {"E"}, {"F"}, {"G"}, {"H"}, {"I"}, {"J"}, {"K"}, {"L"}, {"M"},
    {"N"}, {"O"}, {"P"}, {"Q"}, {"R"}, {"S"}, {"T"}, {"U"}, {"V"}, {"W"}, {"X"}, {"Y"}, {"Z"},
    {"0"}, {"1"}, {"2"}, {"3"}, {"4"}, {"5"}, {"6"}, {"7"}, {"8"}, {"9"},
    {"F1"}, {"F2"}, {"F3"}, {"F4"}, {"F5"}, {"F6"}, {"F7"}, {"F8"}, {"F9"},
    {"F10", 1.25f}, {"F11", 1.25f}, {"F12", 1.25f},
    {"SPACE", 3.0f}, {"ENTER", 2.0f}, {"ESC", 1.4f}, {"TAB", 1.4f}, {"SHIFT", 2.0f}, {"CTRL", 1.6f}, {"ALT", 1.4f},
    {"BACK", 1.7f}, {"DEL", 1.4f},
    {}, {}, {}, {},                           // Arrow keys
    {{}, ClusterAspect}, {{}, ClusterAspect}, // Wasd, ArrowKeys
    {{}, MouseAspect}, {{}, MouseAspect}, {{}, MouseAspect}, {{}, MouseAspect}, // Mouse and its buttons
    {{}, 1.0f}, {{}, 1.0f},                   // MouseWheel, MouseMove
});
// clang-format on
static_assert(Keys.size() == KeyPromptCount);

const KeyRow& RowOf(KeyPrompt prompt) { return Keys[static_cast<std::size_t>(prompt)]; }

// The largest rectangle of this aspect, centred in 'box'.
FloatRect Fitted(const FloatRect& box, float aspect) {
    const float height = std::min(box.height, box.width / aspect);
    return FloatRect::FromCenter(box.Center(), {height * aspect, height});
}

std::string_view GlyphOf(char letter) {
    if (letter >= 'A' && letter <= 'Z') return Letters[static_cast<std::size_t>(letter - 'A')];
    if (letter >= '0' && letter <= '9') return Digits[static_cast<std::size_t>(letter - '0')];
    return {};
}

void StrokeGlyph(Painter& paint, std::string_view glyph, Float2 topLeft, float cell, Color4 color) {
    constexpr std::size_t MostPoints = 16;
    const LineStyle line{
        .width = cell * StrokeOfCell, .color = color, .cap = StrokeCap::Round, .join = StrokeJoin::Round};
    std::array<Float2, MostPoints> points{};
    std::size_t count = 0;
    const auto flush = [&] {
        if (count > 1) paint.Polyline(std::span<const Float2>(points).first(count), line);
        count = 0;
    };
    for (std::size_t i = 0; i < glyph.size(); ++i) {
        if (glyph[i] == '|') {
            flush();
        } else if (glyph[i] != ' ') {
            const Float2 cellAt{static_cast<float>(glyph[i] - '0'), static_cast<float>(glyph[i + 1] - '0')};
            points[count++] = topLeft + cellAt * cell;
            ++i;
        }
    }
    flush();
}

// A word centred on 'area', as high as the area allows and shrunk to fit its width.
void StrokeWord(Painter& paint, std::string_view word, const FloatRect& area, Color4 color) {
    const float count = static_cast<float>(word.size());
    const float units = count * GlyphWidth + (count - 1.0f) * GlyphGap;
    const float cell = std::min(area.height / GlyphHeight, area.width / units);
    const Float2 center = area.Center();
    Float2 at{center.x - units * cell * 0.5f, center.y - GlyphHeight * cell * 0.5f};
    for (const char letter : word) {
        StrokeGlyph(paint, GlyphOf(letter), at, cell, color);
        at.x += (GlyphWidth + GlyphGap) * cell;
    }
}

// Solid arrow pointing along 'direction' (a unit axis), 'size' from its centre to its tip.
void Arrow(Painter& paint, Float2 center, Float2 direction, float size, Color4 color) {
    const Float2 across = Perpendicular(direction);
    const auto at = [&](float along, float aside) { return center + (direction * along + across * aside) * size; };
    const std::array<Float2, 7> arrow{at(1.0f, 0.0f),    at(0.05f, 0.85f),  at(0.05f, 0.34f), at(-0.95f, 0.34f),
                                      at(-0.95f, -0.34f), at(0.05f, -0.34f), at(0.05f, -0.85f)};
    paint.Polygon(arrow, Fill(color));
}

// The key without its label; returns the top, where the label goes.
FloatRect KeyBody(Painter& paint, const FloatRect& rect, float pressed, const KeyPromptLook& look) {
    const float edge = rect.height * EdgeShare;
    const float sink = edge * PressDepth * pressed;
    const float corner = rect.height * CornerShare;
    paint.RoundRect({rect.x, rect.y + sink, rect.width, rect.height - sink}, corner, Fill(look.side));
    const FloatRect top{rect.x, rect.y + sink, rect.width, rect.height - edge};
    paint.RoundRect(top, corner, Fill(look.cap));
    return top;
}

void LabelledKey(Painter& paint, const FloatRect& rect, std::string_view label, float pressed,
                 const KeyPromptLook& look) {
    const FloatRect top = KeyBody(paint, rect, pressed, look);
    const float margin = top.height * LabelMargin;
    const float high = top.height * LabelShare;
    StrokeWord(paint, label, FloatRect::FromCenter(top.Center(), {top.width - margin * 2.0f, high}), look.label);
}

void ArrowKey(Painter& paint, const FloatRect& rect, Float2 direction, float pressed, const KeyPromptLook& look) {
    const FloatRect top = KeyBody(paint, rect, pressed, look);
    Arrow(paint, top.Center(), direction, top.height * 0.3f, look.label);
}

// Four keys as on a keyboard: one over the middle of three. 'key' draws each, slot 0 at the top, then left to right.
template <typename DrawKey>
void Cluster(const FloatRect& rect, const DrawKey& key) {
    const float side = rect.height / (2.0f + ClusterGap);
    const float step = side * (1.0f + ClusterGap);
    const float low = rect.y + step;
    key(0, FloatRect{rect.x + step, rect.y, side, side});
    for (int i = 0; i < 3; ++i)
        key(i + 1, FloatRect{rect.x + step * static_cast<float>(i), low, side, side});
}

enum class MousePart { None, Left, Right, Wheel };

// A mouse filling 'rect'; 'part' stands out in the accent colour.
void DrawMouse(Painter& paint, const FloatRect& rect, MousePart part, float pressed, const KeyPromptLook& look) {
    const float line = rect.width * 0.07f;
    const FloatRect body = rect.Inset(line * 0.5f);
    const float radius = body.width * 0.5f;
    const float centerX = body.Center().x;
    const float upper = body.y + radius;
    const float lower = body.Bottom() - radius;
    const float seam = rect.y + rect.height * MouseButtons;
    const auto around = [&](float y, float degrees) {
        return Float2{centerX, y} + Float2::FromAngle(Radians(degrees), radius);
    };
    const Color4 accent = Mix(look.accent, look.label, pressed * 0.35f);

    std::array<Float2, MouseArcPoints * 2> outline{};
    for (std::size_t i = 0; i < MouseArcPoints; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(MouseArcPoints - 1);
        outline[i] = around(upper, 180.0f + 180.0f * t);
        outline[MouseArcPoints + i] = around(lower, 180.0f * t);
    }
    paint.Polygon(outline, Fill(look.cap));

    if (part == MousePart::Left || part == MousePart::Right) {
        const float side = part == MousePart::Left ? -1.0f : 1.0f;
        std::array<Float2, MouseArcPoints + 2> button{};
        for (std::size_t i = 0; i < MouseArcPoints; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(MouseArcPoints - 1);
            button[i] = around(upper, 270.0f + side * 90.0f * (1.0f - t));
        }
        button[MouseArcPoints] = {centerX, seam};
        button[MouseArcPoints + 1] = {centerX + side * radius, seam};
        paint.Polygon(button, Fill(accent));
    }

    paint.Polygon(outline, Outline(line, look.side));
    const LineStyle seamLine{.width = line * 0.8f, .color = look.side};
    paint.Line({body.x, seam}, {body.Right(), seam}, seamLine);
    paint.Line({centerX, body.y}, {centerX, seam}, seamLine);
    const float wheelY = rect.y + rect.height * 0.25f;
    const Float2 wheel{rect.width * 0.09f, rect.height * 0.1f};
    paint.RoundRect(FloatRect::FromCenter({centerX, wheelY}, wheel * 2.0f), wheel.x,
                    Fill(part == MousePart::Wheel ? accent : look.side));
}

void MouseWithWheelArrows(Painter& paint, const FloatRect& rect, float pressed, const KeyPromptLook& look) {
    const float arrowSize = rect.height * 0.15f;
    DrawMouse(paint, {rect.x, rect.y, rect.height * MouseAspect, rect.height}, MousePart::Wheel, pressed, look);
    const float x = rect.Right() - arrowSize * 0.85f;
    const Color4 accent = Mix(look.accent, look.label, pressed * 0.35f);
    Arrow(paint, {x, rect.y + rect.height * 0.28f}, {0.0f, -1.0f}, arrowSize, accent);
    Arrow(paint, {x, rect.y + rect.height * 0.72f}, {0.0f, 1.0f}, arrowSize, accent);
}

void MouseWithMoveArrows(Painter& paint, const FloatRect& rect, float pressed, const KeyPromptLook& look) {
    constexpr std::array<Float2, 4> Directions{{{0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}}};
    const Float2 center = rect.Center();
    const float high = rect.height * 0.56f;
    DrawMouse(paint, FloatRect::FromCenter(center, {high * MouseAspect, high}), MousePart::None, pressed, look);
    const float arrowSize = rect.height * 0.085f;
    const Color4 accent = Mix(look.accent, look.label, pressed * 0.35f);
    for (const Float2 direction : Directions)
        Arrow(paint, center + direction * (rect.height * 0.5f - arrowSize), direction, arrowSize, accent);
}
} // namespace

float KeyPromptAspect(KeyPrompt prompt) { return prompt == KeyPrompt::Count ? 1.0f : RowOf(prompt).aspect; }

void DrawKeyPrompt(Painter& paint, const FloatRect& box, KeyPrompt prompt, float pressed, const KeyPromptLook& look) {
    if (prompt == KeyPrompt::Count || box.Empty()) return;
    pressed = std::clamp(pressed, 0.0f, 1.0f);
    const FloatRect rect = Fitted(box, RowOf(prompt).aspect);
    switch (prompt) {
    case KeyPrompt::Up   : ArrowKey(paint, rect, {0.0f, -1.0f}, pressed, look); break;
    case KeyPrompt::Down : ArrowKey(paint, rect, {0.0f, 1.0f}, pressed, look); break;
    case KeyPrompt::Left : ArrowKey(paint, rect, {-1.0f, 0.0f}, pressed, look); break;
    case KeyPrompt::Right: ArrowKey(paint, rect, {1.0f, 0.0f}, pressed, look); break;
    case KeyPrompt::Wasd : {
        constexpr std::array<std::string_view, 4> Labels{"W", "A", "S", "D"};
        Cluster(rect, [&](int slot, const FloatRect& key) {
            LabelledKey(paint, key, Labels[static_cast<std::size_t>(slot)], pressed, look);
        });
        break;
    }
    case KeyPrompt::ArrowKeys: {
        constexpr std::array<Float2, 4> Directions{{{0.0f, -1.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}}};
        Cluster(rect, [&](int slot, const FloatRect& key) {
            ArrowKey(paint, key, Directions[static_cast<std::size_t>(slot)], pressed, look);
        });
        break;
    }
    case KeyPrompt::Mouse      : DrawMouse(paint, rect, MousePart::None, pressed, look); break;
    case KeyPrompt::MouseLeft  : DrawMouse(paint, rect, MousePart::Left, pressed, look); break;
    case KeyPrompt::MouseRight : DrawMouse(paint, rect, MousePart::Right, pressed, look); break;
    case KeyPrompt::MouseMiddle: DrawMouse(paint, rect, MousePart::Wheel, pressed, look); break;
    case KeyPrompt::MouseWheel : MouseWithWheelArrows(paint, rect, pressed, look); break;
    case KeyPrompt::MouseMove  : MouseWithMoveArrows(paint, rect, pressed, look); break;
    default                    : LabelledKey(paint, rect, RowOf(prompt).label, pressed, look); break;
    }
}
