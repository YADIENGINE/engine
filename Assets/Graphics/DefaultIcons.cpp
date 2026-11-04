#include "Graphics/DefaultIcons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Geometry/FloatRect.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr float FillAlpha = 0.3f; // Inner fill of line icons
constexpr float LineShare = 0.08f; // An icon's own stroke width, of its size
constexpr float MinLine = 1.5f;    // Below this a small icon's strokes break up
constexpr Color4 Black{0, 0, 0, 255};
constexpr Color4 White{255, 255, 255, 255};
constexpr std::size_t HalfDiscPoints = 25;

// A look with every value decided.
struct IconStyle {
    Color4 color;
    Color4 detail;
    float line;
};

struct IconColors {
    Color4 color;
    Color4 detail{0, 0, 0, 0}; // Fully transparent: the shape's own fallback, usually a dark detail
};

constexpr Color4 Yellow{255, 220, 60, 255};
constexpr Color4 Gold{255, 205, 87, 255};
constexpr Color4 Sand{222, 184, 110, 255};
constexpr Color4 Cyan{40, 230, 255, 255};
constexpr Color4 Sky{100, 210, 255, 255};
constexpr Color4 Teal{60, 230, 220, 255};
constexpr Color4 Purple{160, 90, 255, 255};
constexpr Color4 Pink{255, 60, 180, 255};
constexpr Color4 Rose{255, 70, 90, 255};
constexpr Color4 Orange{255, 150, 60, 255};
constexpr Color4 Mint{80, 240, 150, 255};
constexpr Color4 Green{60, 255, 120, 255};
constexpr Color4 Moss{190, 215, 120, 255};
constexpr Color4 Ink{11, 9, 26, 255};

// One row per icon, in Icon's order.
constexpr auto OwnColors = std::to_array<IconColors>({
    {Yellow},                                         // Star
    {{255, 240, 200, 255}, {255, 255, 255, 200}},     // Egg
    {{90, 150, 255, 255}},                            // Hourglass
    {{200, 195, 230, 255}},                           // Plus
    {{255, 120, 170, 255}},                           // Minus
    {Teal},                                           // UpArrow
    {{255, 80, 80, 255}},                             // Chevrons
    {{230, 45, 55, 255}},                             // Apple
    {{235, 70, 110, 255}},                            // Magnet
    {{200, 225, 245, 255}, Ink},                      // Ghost
    {{190, 160, 130, 255}, {70, 50, 35, 255}},        // Brick
    {{80, 210, 255, 255}},                            // Rain
    {Sand},                                           // Clock
    {Cyan},                                           // Book
    {Sand},                                           // Shield
    {Rose},                                           // HornedEye
    {Gold},                                           // Sun
    {Mint},                                           // Gem
    {Yellow},                                         // Bolt
    {Teal},                                           // Rune
    {Moss},                                           // Mountain
    {{190, 186, 220, 255}},                           // Lock
    {{255, 200, 45, 255}},                            // Coin
    {{170, 135, 255, 255}},                           // Dice
    {Orange},                                         // Cards
    {{90, 255, 140, 255}},                            // LoopArrow
    {{255, 215, 70, 255}},                            // Cloud
    {{255, 140, 40, 255}},                            // StormCloud
    {{255, 60, 80, 255}},                             // Tornado
    {White, Yellow},                                  // CrownedStar
    {Orange},                                         // Compass
    {{190, 90, 255, 255}},                            // Comet
    {Cyan},                                           // Gear
    {Purple},                                         // Heart
    {{255, 50, 60, 255}},                             // Power
    {Pink},                                           // Lotus
    {Gold},                                           // Goblet
    {Cyan},                                           // Stopwatch
    {Mint},                                           // Sack
    {Teal},                                           // Wings
    {Sky},                                            // Flag
    {{235, 235, 245, 255}},                           // Feather
    {Gold, {255, 244, 211, 255}},                     // Chest
    {Moss},                                           // Pickaxe
    {Sky},                                            // Anchor
    {Mint},                                           // Swords
    {Cyan},                                           // Coil
    {Green},                                          // Clover
    {Purple},                                         // Swirl
    {Sky},                                            // TurnArrow
    {Green},                                          // Copy
    {Yellow},                                         // Cat
    {Teal},                                           // Couch
    {{255, 95, 65, 255}},                             // Pedal
    {{190, 160, 255, 255}},                           // Mask
    {Mint},                                           // Hen
    {Cyan},                                           // Tire
    {Rose},                                           // Bubble
    {Rose},                                           // Scroll
    {Orange, Yellow},                                 // Flame
    {Mint},                                           // Play
    {Sky},                                            // Pause
    {Teal},                                           // Restart
    {Green},                                          // Check
    {Rose},                                           // Cross
    {Orange, Ink},                                    // Home
    {Sky},                                            // Info
    {Yellow, Ink},                                    // Warning
    {Teal},                                           // SoundOn
    {{190, 186, 220, 255}, Rose},                     // SoundOff
    {Pink},                                           // Music
    {Sky},                                            // Save
    {Rose},                                           // Trash
    {{205, 225, 245, 255}, Gold},                     // Sword
    {{215, 160, 95, 255}, {235, 235, 245, 255}},      // Bow
    {Pink, Sand},                                     // Potion
    {{125, 122, 155, 255}, Orange},                   // Bomb
    {{238, 232, 215, 255}, Ink},                      // Skull
    {Gold},                                           // Key
    {Gold, Rose},                                     // Crown
    {Gold, {255, 244, 211, 255}},                     // Trophy
    {{240, 235, 190, 255}},                           // Moon
    {{70, 170, 255, 255}},                            // Droplet
    {{170, 225, 255, 255}},                           // Snowflake
    {{110, 210, 90, 255}, {35, 95, 50, 255}},         // Leaf
    {Teal, Ink},                                      // Eye
    {Rose},                                           // Target
});
static_assert(OwnColors.size() == IconCount);

IconStyle Resolved(Icon icon, float size, const IconLook& look) {
    const IconColors& own = OwnColors[static_cast<std::size_t>(icon)];
    const Color4 color = look.color.value_or(own.color);
    return {color, look.detail.value_or(ScaleAlpha(own.detail, static_cast<float>(color.a) / 255.0f)),
            look.line.value_or(std::max(MinLine, size * LineShare))};
}

// --- Drawing helpers (angles in radians: 0 points right, clockwise on screen) ---

void Segment(Painter& p, Float2 a, Float2 b, float width, Color4 color, StrokeCap cap = StrokeCap::Butt) {
    p.Line(a, b, {.width = width, .color = color, .cap = cap});
}
void Chain(Painter& p, std::span<const Float2> points, float width, Color4 color, StrokeJoin join = StrokeJoin::Miter,
           StrokeCap cap = StrokeCap::Butt) {
    p.Polyline(points, {.width = width, .color = color, .cap = cap, .join = join});
}
void Disc(Painter& p, Float2 center, float radius, Color4 color) { p.Circle(center, radius, Fill(color)); }
void Tri(Painter& p, Float2 a, Float2 b, Float2 c, Color4 color) { p.Triangle(a, b, c, Fill(color)); }
void Box(Painter& p, const FloatRect& rect, Color4 color) { p.Rect(rect, Fill(color)); }
// Full ring between the two radii.
void Ring(Painter& p, Float2 center, float inner, float outer, Color4 color) {
    p.Circle(center, outer, Outline(outer - inner, color, StrokeAlign::Inside));
}
// Part of a ring between the two radii, from 'start' to 'end' (radians, clockwise).
void Band(Painter& p, Float2 center, float inner, float outer, float start, float end, Color4 color) {
    p.Arc(center, (inner + outer) * 0.5f, start, end - start, {.width = outer - inner, .color = color});
}
// Half disc below its diameter (a bowl).
void HalfDisc(Painter& p, Float2 center, float radius, Color4 color) {
    std::array<Float2, HalfDiscPoints> points{};
    for (std::size_t i = 0; i < points.size(); ++i)
        points[i] =
            center + Float2::FromAngle(Pi * static_cast<float>(i) / static_cast<float>(points.size() - 1), radius);
    p.Polygon(points, Fill(color));
}

Color4 Faded(const IconStyle& style, float alpha = FillAlpha) { return ScaleAlpha(style.color, alpha); }
// A fixed colour that still fades with the icon.
Color4 Tinted(Color4 color, const IconStyle& style) {
    return ScaleAlpha(color, static_cast<float>(style.color.a) / 255.0f);
}
// The dark detail inside an icon (eye, keyhole): 'detail', or translucent black when it has none.
Color4 DetailOr(const IconStyle& style) {
    return style.detail.a > 0 ? style.detail : Tinted(WithAlpha(Black, 0.6f), style);
}
// The second colour of a two-coloured icon (a guard, a cork, a spark): 'detail', or the icon's own without one.
Color4 AccentOr(const IconStyle& style) { return style.detail.a > 0 ? style.detail : style.color; }
// Translucent fill with the icon's outline.
ShapeStyle Framed(const IconStyle& style, float fillAlpha = FillAlpha) {
    return {.fill = Faded(style, fillAlpha), .stroke = {style.line, style.color}};
}
void FramedDisc(Painter& p, Float2 center, float radius, const IconStyle& style, float fillAlpha = FillAlpha) {
    p.Circle(center, radius,
             {.fill = Faded(style, fillAlpha), .stroke = {style.line, style.color, StrokeAlign::Inside}});
}
// Unit direction: 0 radians up, growing clockwise.
Float2 Dir(float angle) { return {std::sin(angle), -std::cos(angle)}; }
// Leaf (rhombus) growing from 'base' along 'dir'; widest at 'widest' of its length.
std::array<Float2, 4> Leaf(Float2 base, Float2 dir, float length, float width, float widest) {
    const Float2 perp = Perpendicular(dir);
    const Float2 mid = base + dir * (length * widest);
    return {base, mid + perp * width, base + dir * length, mid - perp * width};
}

// --- Solid icons ---

void Hourglass(Painter& p, Float2 c, float s, Color4 color) {
    const float w = s * 0.83f;
    Tri(p, {c.x - w, c.y - s}, c, {c.x + w, c.y - s}, color);
    Tri(p, c, {c.x - w, c.y + s}, {c.x + w, c.y + s}, color);
}

void Plus(Painter& p, Float2 c, float s, Color4 color) {
    const float t = s * 0.47f * 0.5f;
    const std::array<Float2, 12> cross{{{c.x - t, c.y - s},
                                        {c.x + t, c.y - s},
                                        {c.x + t, c.y - t},
                                        {c.x + s, c.y - t},
                                        {c.x + s, c.y + t},
                                        {c.x + t, c.y + t},
                                        {c.x + t, c.y + s},
                                        {c.x - t, c.y + s},
                                        {c.x - t, c.y + t},
                                        {c.x - s, c.y + t},
                                        {c.x - s, c.y - t},
                                        {c.x - t, c.y - t}}};
    p.Polygon(cross, Fill(color));
}

void Minus(Painter& p, Float2 c, float s, Color4 color) {
    const float thick = s * 0.47f;
    Box(p, {c.x - s, c.y - thick * 0.5f, s * 2.0f, thick}, color);
}

void UpArrow(Painter& p, Float2 c, float s, Color4 color) {
    const float w = s * 0.92f;
    const float shaft = s * 0.3f;
    const std::array<Float2, 7> arrow{{{c.x, c.y - s * 1.08f},
                                       {c.x + w, c.y},
                                       {c.x + shaft, c.y},
                                       {c.x + shaft, c.y + s},
                                       {c.x - shaft, c.y + s},
                                       {c.x - shaft, c.y},
                                       {c.x - w, c.y}}};
    p.Polygon(arrow, Fill(color));
}

void Chevrons(Painter& p, Float2 c, float s, Color4 color) {
    const float h = s * 0.83f;
    for (const float dx : {-s * 0.5f, s * 0.33f}) {
        const std::array<Float2, 3> chevron{
            {{c.x + dx - h * 0.5f, c.y - h}, {c.x + dx + h * 0.5f, c.y}, {c.x + dx - h * 0.5f, c.y + h}}};
        Chain(p, chevron, s * 0.33f, color);
    }
}

void Apple(Painter& p, Float2 c, float s, Color4 color) {
    Disc(p, {c.x, c.y + s * 0.17f}, s * 0.75f, color);
    Box(p, {c.x - s * 0.1f, c.y - s, s * 0.2f, s * 0.5f}, color);
}

void Magnet(Painter& p, Float2 c, float s, Color4 color) {
    const float inner = s * 0.5f;
    Band(p, c, inner, s, 0.0f, Radians(180.0f), color);
    Box(p, {c.x - s, c.y - s * 0.75f, s - inner, s * 0.75f}, color);
    Box(p, {c.x + inner, c.y - s * 0.75f, s - inner, s * 0.75f}, color);
}

void Ghost(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float w = s * 0.83f;
    const float top = c.y - s * 0.17f;
    Disc(p, {c.x, top}, w, style.color);
    Box(p, {c.x - w, top, w * 2.0f, s * 0.92f}, style.color);
    Disc(p, {c.x - w * 0.4f, top}, s * 0.2f, style.detail);
    Disc(p, {c.x + w * 0.4f, top}, s * 0.2f, style.detail);
}

void Brick(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float h = s * 0.75f;
    Box(p, {c.x - s, c.y - h, s * 2.0f, h * 2.0f}, style.color);
    const std::array<Float2, 3> crack{{{c.x - s * 0.2f, c.y - h}, {c.x + s * 0.15f, c.y}, {c.x - s * 0.1f, c.y + h}}};
    Chain(p, crack, s * 0.2f, style.detail);
}

void Rain(Painter& p, Float2 c, float s, Color4 color) {
    constexpr std::array<Float2, 5> Drops{
        {{-0.58f, -0.5f}, {0.08f, -0.08f}, {0.67f, -0.58f}, {-0.33f, 0.58f}, {0.5f, 0.5f}}};
    for (const Float2 drop : Drops)
        Disc(p, c + drop * s, s * 0.28f, color);
}

// Round-topped cloud with a flat base.
void Cloud(Painter& p, Float2 c, float s, Color4 color) {
    Disc(p, {c.x - s * 0.5f, c.y + s * 0.1f}, s * 0.4f, color);
    Disc(p, {c.x + s * 0.05f, c.y - s * 0.15f}, s * 0.55f, color);
    Disc(p, {c.x + s * 0.55f, c.y + s * 0.15f}, s * 0.38f, color);
    Box(p, {c.x - s * 0.5f, c.y + s * 0.1f, s * 1.05f, s * 0.43f}, color);
}

// Star wearing a three-pointed crown; the crown in 'detail' (the star's colour without one).
void CrownedStar(Painter& p, Float2 c, float s, const IconStyle& style) {
    DrawStar(p, {c.x, c.y + s * 0.2f}, s * 0.78f, Fill(style.color));
    const Color4 crown = style.detail.a > 0 ? style.detail : style.color;
    const float base = c.y - s * 0.55f;
    const float notch = c.y - s * 0.75f;
    const float tip = c.y - s;
    const float w = s * 0.5f;
    const std::array<Float2, 7> outline{{{c.x - w, base},
                                         {c.x - w, tip},
                                         {c.x - w * 0.5f, notch},
                                         {c.x, tip},
                                         {c.x + w * 0.5f, notch},
                                         {c.x + w, tip},
                                         {c.x + w, base}}};
    p.Polygon(outline, Fill(crown));
}

// Two circles and a pointed bottom.
void Heart(Painter& p, Float2 c, float s, Color4 color) {
    const float radius = s * 0.52f;
    Disc(p, {c.x - s * 0.48f, c.y - s * 0.3f}, radius, color);
    Disc(p, {c.x + s * 0.48f, c.y - s * 0.3f}, radius, color);
    Tri(p, {c.x - s * 0.96f, c.y - s * 0.12f}, {c.x + s * 0.96f, c.y - s * 0.12f}, {c.x, c.y + s * 0.95f}, color);
}

// Four heart-shaped leaves and a stem.
void Clover(Painter& p, Float2 c, float s, Color4 color) {
    const Float2 mid{c.x, c.y - s * 0.1f};
    Segment(p, mid, {c.x + s * 0.35f, c.y + s}, s * 0.14f, color);
    for (int i = 0; i < 4; ++i) {
        const Float2 d = Dir(Radians(45.0f + 90.0f * static_cast<float>(i)));
        const Float2 across = Perpendicular(d);
        const Float2 leaf = mid + d * (s * 0.42f);
        for (const float side : {-1.0f, 1.0f})
            Disc(p, leaf + across * (side * s * 0.17f), s * 0.26f, color);
        Tri(p, mid, leaf + across * (s * 0.36f), leaf - across * (s * 0.36f), color);
    }
}

// Rises from the left, curls right in a quarter circle, ends in an arrowhead.
void TurnArrow(Painter& p, Float2 c, float s, Color4 color) {
    const float thick = s * 0.36f;
    const Float2 bend{c.x - s * 0.05f, c.y - s * 0.1f};
    const float outer = s * 0.68f;
    const float mid = outer - thick * 0.5f;
    Box(p, {bend.x - outer, bend.y, thick, c.y + s - bend.y}, color);
    Band(p, bend, outer - thick, outer, Radians(180.0f), Radians(270.0f), color);
    const float barY = bend.y - outer;
    const float headX = c.x + s * 0.35f;
    Box(p, {bend.x, barY, headX - bend.x, thick}, color);
    const float tipY = bend.y - mid;
    Tri(p, {c.x + s * 0.98f, tipY}, {headX, tipY - s * 0.42f}, {headX, tipY + s * 0.42f}, color);
}

// --- Line icons ---

void Clock(Painter& p, Float2 c, float s, const IconStyle& style) {
    FramedDisc(p, c, s, style);
    const std::array<Float2, 3> hands{{{c.x, c.y - s * 0.65f}, c, {c.x + s * 0.45f, c.y + s * 0.2f}}};
    Chain(p, hands, style.line * 1.4f, style.color, StrokeJoin::Round);
    Disc(p, c, style.line * 1.4f, style.color);
}

void Book(Painter& p, Float2 c, float s, const IconStyle& style) {
    const FloatRect cover{c.x - s, c.y - s * 0.7f, s * 2.0f, s * 1.4f};
    p.RoundRect(cover, s * 0.084f, Framed(style));
    Segment(p, {c.x, cover.y}, {c.x, cover.Bottom()}, style.line, style.color);
    for (int i = -1; i <= 1; ++i) {
        const float y = c.y + static_cast<float>(i) * s * 0.32f;
        Segment(p, {c.x - s * 0.8f, y}, {c.x - s * 0.2f, y}, style.line * 0.67f, style.color);
        Segment(p, {c.x + s * 0.2f, y}, {c.x + s * 0.8f, y}, style.line * 0.67f, style.color);
    }
}

void Shield(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 5> outline{{{c.x - s, c.y - s * 0.75f},
                                         {c.x + s, c.y - s * 0.75f},
                                         {c.x + s * 0.8f, c.y + s * 0.3f},
                                         {c.x, c.y + s},
                                         {c.x - s * 0.8f, c.y + s * 0.3f}}};
    p.Polygon(outline, Framed(style));
    Segment(p, {c.x, c.y - s * 0.45f}, {c.x, c.y + s * 0.5f}, style.line * 1.25f, style.color);
    Segment(p, {c.x - s * 0.45f, c.y}, {c.x + s * 0.45f, c.y}, style.line * 1.25f, style.color);
}

void HornedEye(Painter& p, Float2 c, float s, const IconStyle& style) {
    Tri(p, {c.x - s * 0.85f, c.y - s}, {c.x - s * 0.65f, c.y - s * 0.15f}, {c.x - s * 0.2f, c.y - s * 0.5f},
        style.color);
    Tri(p, {c.x + s * 0.85f, c.y - s}, {c.x + s * 0.2f, c.y - s * 0.5f}, {c.x + s * 0.65f, c.y - s * 0.15f},
        style.color);
    FramedDisc(p, c, s * 0.7f, style, 0.25f);
    Disc(p, c, s * 0.3f, style.color);
}

void Sun(Painter& p, Float2 c, float s, const IconStyle& style) {
    Disc(p, c, s * 0.45f, style.color);
    for (int i = 0; i < 8; ++i) {
        const Float2 dir = Float2::FromAngle(Radians(static_cast<float>(i) * 45.0f));
        Segment(p, c + dir * (s * 0.6f), c + dir * s, style.line, style.color);
    }
}

// A cloud with lightning striking below it.
void StormCloud(Painter& p, Float2 c, float s, const IconStyle& style) {
    Cloud(p, {c.x, c.y - s * 0.35f}, s * 0.85f, style.color);
    const std::array<Float2, 4> bolt{{{c.x + s * 0.15f, c.y + s * 0.05f},
                                      {c.x - s * 0.2f, c.y + s * 0.55f},
                                      {c.x + s * 0.15f, c.y + s * 0.5f},
                                      {c.x - s * 0.1f, c.y + s}}};
    Chain(p, bolt, style.line * 1.4f, style.color);
}

// Lines narrowing downwards and swaying sideways.
void Tornado(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr int Lines = 5;
    for (int i = 0; i < Lines; ++i) {
        const float t = static_cast<float>(i) / (Lines - 1); // 0: top, 1: tip
        const float y = c.y + s * (-0.85f + 1.7f * t);
        const float half = s * (1.0f - 0.8f * t);
        const float x = c.x + s * 0.25f * std::sin(t * Pi * 1.2f);
        Segment(p, {x - half, y}, {x + half, y}, style.line * 1.4f, style.color, StrokeCap::Round);
    }
}

// Bright head at the top right, tail thinning to the bottom left.
void Comet(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 head{c.x + s * 0.45f, c.y - s * 0.45f};
    constexpr int Streaks = 3;
    for (int i = 0; i < Streaks; ++i) {
        const float offset = (static_cast<float>(i) - 1.0f) * s * 0.28f * 0.7f;
        const float reach = i == 1 ? 1.0f : 0.7f;
        Segment(p, head + Float2{offset, offset}, Float2{c.x - s * reach + offset, c.y + s * reach + offset},
                style.line * (i == 1 ? 1.6f : 1.0f), Faded(style, i == 1 ? 0.8f : 0.5f));
    }
    Disc(p, head, s * 0.42f, style.color);
    Disc(p, head - Float2{s * 0.12f, s * 0.12f}, s * 0.14f, Tinted(WithAlpha(White, 0.7f), style));
}

// Needle in a ring: the north end solid, the south end faint.
void Compass(Painter& p, Float2 c, float s, const IconStyle& style) {
    FramedDisc(p, c, s, style);
    const float w = s * 0.28f;
    const Float2 north{c.x + s * 0.45f, c.y - s * 0.6f};
    const Float2 south{c.x - s * 0.45f, c.y + s * 0.6f};
    const Float2 left{c.x - w * 0.8f, c.y - w * 0.6f};
    const Float2 right{c.x + w * 0.8f, c.y + w * 0.6f};
    Tri(p, north, left, right, style.color);
    Tri(p, south, left, right, Faded(style, 0.55f));
    Disc(p, c, style.line, style.color);
}

void Gem(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 left{c.x - s * 0.75f, c.y - s * 0.25f};
    const Float2 right{c.x + s * 0.75f, c.y - s * 0.25f};
    const std::array<Float2, 4> outline{{{c.x, c.y - s}, right, {c.x, c.y + s}, left}};
    p.Polygon(outline, Framed(style));
    Segment(p, left, right, style.line, style.color);
}

void Bolt(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 4> bolt{{{c.x + s * 0.3f, c.y - s},
                                      {c.x - s * 0.4f, c.y + s * 0.05f},
                                      {c.x + s * 0.35f, c.y - s * 0.05f},
                                      {c.x - s * 0.3f, c.y + s}}};
    Chain(p, bolt, style.line * 1.4f, style.color);
    Disc(p, {c.x + s * 0.7f, c.y + s * 0.6f}, style.line, style.color);
    Disc(p, {c.x - s * 0.7f, c.y - s * 0.6f}, style.line, style.color);
}

void Rune(Painter& p, Float2 c, float s, const IconStyle& style) {
    Ring(p, c, s - style.line, s, style.color);
    std::array<Float2, 3> triangle{};
    for (std::size_t k = 0; k < triangle.size(); ++k)
        triangle[k] = c + Float2::FromAngle(Radians(static_cast<float>(k) * 120.0f - 90.0f), s * 0.7f);
    p.Polyline(triangle, {.width = style.line, .color = style.color}, true);
    Disc(p, c, style.line * 1.2f, style.color);
}

void Mountain(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 baseLeft{c.x - s, c.y + s * 0.6f};
    const Float2 baseRight{c.x + s, c.y + s * 0.6f};
    const Float2 peak{c.x - s * 0.2f, c.y - s * 0.8f};
    const Float2 peak2{c.x + s * 0.5f, c.y - s * 0.2f};
    Tri(p, baseLeft, peak, {c.x + s * 0.35f, baseLeft.y}, Faded(style));
    const std::array<Float2, 3> left{{baseLeft, peak, {c.x + s * 0.2f, c.y + s * 0.05f}}};
    const std::array<Float2, 3> right{{{c.x + s * 0.1f, c.y + s * 0.15f}, peak2, baseRight}};
    Chain(p, left, style.line, style.color);
    Chain(p, right, style.line, style.color);
    Segment(p, baseLeft, baseRight, style.line, style.color);
}

// Padlock: half-ring shackle, body, keyhole.
void Lock(Painter& p, Float2 c, float s, const IconStyle& style) {
    const FloatRect body{c.x - s * 0.8f, c.y - s * 0.1f, s * 1.6f, s * 1.1f};
    const float shackle = style.line * 1.3f;
    Band(p, {c.x, body.y}, s * 0.45f, s * 0.45f + shackle, Radians(180.0f), Radians(360.0f), style.color);
    p.RoundRect(body, s * 0.11f, Framed(style));
    const Float2 hole{c.x, body.y + body.height * 0.42f};
    Disc(p, hole, s * 0.16f, style.color);
    Segment(p, hole, {hole.x, hole.y + s * 0.3f}, style.line * 1.2f, style.color);
}

// Coin: double-rimmed disc with a star.
void Coin(Painter& p, Float2 c, float s, const IconStyle& style) {
    FramedDisc(p, c, s, style);
    Ring(p, c, s * 0.72f - style.line * 0.5f, s * 0.72f, style.color);
    DrawStar(p, c, s * 0.42f, Fill(style.color));
}

// Die: rounded square showing five.
void Dice(Painter& p, Float2 c, float s, const IconStyle& style) {
    p.RoundRect(FloatRect::FromCenter(c, {s * 1.7f, s * 1.7f}), s * 0.255f, Framed(style));
    const float d = s * 0.45f;
    for (const Float2 offset : {Float2{-d, -d}, Float2{d, -d}, Float2{}, Float2{-d, d}, Float2{d, d}})
        Disc(p, c + offset, s * 0.16f, style.color);
}

// Corners of a rectangle turned 'angle' radians around the middle of its bottom edge (clockwise from the top left).
std::array<Float2, 4> TurnedCard(Float2 pivot, float width, float height, float angle) {
    const auto at = [&](float x, float y) { return pivot + Rotated({x, y}, angle); };
    return {at(-width * 0.5f, -height), at(width * 0.5f, -height), at(width * 0.5f, 0.0f), at(-width * 0.5f, 0.0f)};
}

// Three cards fanned out from the bottom; a diamond on the front one.
void Cards(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 pivot{c.x, c.y + s * 0.85f};
    const float height = s * 1.55f;
    for (const float degrees : {-24.0f, 24.0f, 0.0f}) {
        const std::array<Float2, 4> card = TurnedCard(pivot, s * 0.95f, height, Radians(degrees));
        if (degrees == 0.0f) p.Polygon(card, Fill(Tinted(WithAlpha(Black, 0.55f), style)));
        p.Polygon(card, Framed(style));
    }
    const Float2 mid{c.x, pivot.y - height * 0.5f};
    const float d = s * 0.22f;
    DrawDiamond(p, mid, {d, d * 1.4f}, Fill(style.color));
}

// An open ring turning around itself, an arrowhead where it ends.
void LoopArrow(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr float Start = 40.0f;
    constexpr float End = 300.0f;
    const float radius = s * 0.8f;
    Disc(p, c, radius, Faded(style, FillAlpha * 0.6f));
    Band(p, c, radius - style.line, radius, Radians(Start), Radians(End), style.color);
    const Float2 normal = Float2::FromAngle(Radians(End));
    const Float2 tangent = Perpendicular(normal);
    const Float2 tip = c + normal * (radius - style.line * 0.5f);
    const float head = s * 0.42f;
    Tri(p, tip + tangent * head, tip + normal * (head * 0.65f), tip - normal * (head * 0.65f), style.color);
    Disc(p, c, style.line * 1.3f, style.color);
}

// Five petals opening from below, a water line under them.
void Lotus(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 base{c.x, c.y + s * 0.6f};
    constexpr std::array<std::pair<float, float>, 5> Petals{
        {{-72.0f, 0.85f}, {72.0f, 0.85f}, {-36.0f, 1.2f}, {36.0f, 1.2f}, {0.0f, 1.5f}}};
    for (const auto& [degrees, length] : Petals)
        p.Polygon(Leaf(base, Dir(Radians(degrees)), s * length, s * 0.24f, 0.5f), Framed(style));
    Segment(p, {c.x - s, c.y + s * 0.82f}, {c.x + s, c.y + s * 0.82f}, style.line, style.color);
}

// Goblet: filled half-round bowl, thin stem, flat foot; a sparkle at the rim.
void Goblet(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 bowl{c.x, c.y - s * 0.45f};
    const float r = s * 0.75f;
    HalfDisc(p, bowl, r, Faded(style));
    HalfDisc(p, {bowl.x, bowl.y + s * 0.12f}, r * 0.72f, Faded(style, 0.65f));
    Band(p, bowl, r - style.line, r, 0.0f, Radians(180.0f), style.color);
    Segment(p, {bowl.x - r, bowl.y}, {bowl.x + r, bowl.y}, style.line, style.color);
    Segment(p, {c.x, bowl.y + r}, {c.x, c.y + s * 0.82f}, style.line * 1.3f, style.color);
    Segment(p, {c.x - s * 0.5f, c.y + s * 0.85f}, {c.x + s * 0.5f, c.y + s * 0.85f}, style.line * 1.5f, style.color);
    DrawStar(p, {c.x + s * 0.45f, bowl.y - s * 0.38f}, s * 0.22f, Fill(style.color));
}

// Stopwatch: dial, crown button, side button, quarter marks and a hand.
void Stopwatch(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 face{c.x, c.y + s * 0.15f};
    const float r = s * 0.82f;
    const auto onFace = [&](Float2 dir, float radius) { return face + dir * radius; };
    FramedDisc(p, face, r, style);
    Box(p, {c.x - s * 0.2f, c.y - s, s * 0.4f, s * 0.18f}, style.color);
    Segment(p, {c.x, c.y - s * 0.84f}, {c.x, face.y - r}, style.line * 1.4f, style.color);
    Segment(p, onFace(Dir(Radians(45.0f)), r), onFace(Dir(Radians(45.0f)), r + s * 0.16f), style.line * 1.6f,
            style.color);
    for (int i = 0; i < 4; ++i) {
        const Float2 d = Dir(Radians(90.0f * static_cast<float>(i)));
        Segment(p, onFace(d, r * 0.6f), onFace(d, r * 0.78f), style.line, style.color);
    }
    Segment(p, face, onFace(Dir(Radians(50.0f)), r * 0.62f), style.line * 1.4f, style.color);
    Disc(p, face, style.line * 1.3f, style.color);
}

// Money bag: round body with a star, tied neck, open mouth.
void Sack(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 body{c.x, c.y + s * 0.28f};
    const float r = s * 0.7f;
    FramedDisc(p, body, r, style);
    const float neck = c.y - s * 0.5f;
    const std::array<Float2, 4> mouth{
        {{c.x - s * 0.22f, neck}, {c.x - s * 0.55f, c.y - s}, {c.x + s * 0.55f, c.y - s}, {c.x + s * 0.22f, neck}}};
    p.Polygon(mouth, Framed(style));
    for (const float side : {-1.0f, 1.0f})
        Segment(p, {c.x + side * s * 0.22f, neck}, {c.x + side * s * 0.3f, body.y - r * 0.9f}, style.line, style.color);
    Segment(p, {c.x - s * 0.34f, neck}, {c.x + s * 0.34f, neck}, style.line * 1.8f, style.color);
    DrawStar(p, {body.x, body.y + s * 0.02f}, s * 0.32f, Fill(style.color));
}

// Wings: three feathers opening to each side, a point upwards in the middle.
void Wings(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr std::array<std::pair<float, float>, 3> Feathers{{{78.0f, 0.88f}, {52.0f, 0.95f}, {26.0f, 0.8f}}};
    const Float2 root{c.x, c.y + s * 0.45f};
    for (const float side : {-1.0f, 1.0f})
        for (const auto& [degrees, length] : Feathers)
            p.Polygon(
                Leaf({root.x + side * s * 0.12f, root.y}, Dir(Radians(side * degrees)), s * length, s * 0.15f, 0.6f),
                Framed(style));
    Disc(p, root, s * 0.16f, style.color);
    Tri(p, {c.x, c.y - s}, {c.x - s * 0.24f, c.y - s * 0.58f}, {c.x + s * 0.24f, c.y - s * 0.58f}, style.color);
}

// Finish flag: pole and a chequered cloth.
void Flag(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr int Columns = 4;
    constexpr int Rows = 3;
    const float pole = c.x - s * 0.75f;
    const FloatRect flag{pole, c.y - s * 0.9f, s * 1.6f, s * 1.05f};
    const float w = flag.width / Columns;
    const float h = flag.height / Rows;
    for (int row = 0; row < Rows; ++row)
        for (int column = 0; column < Columns; ++column)
            Box(p, {flag.x + w * static_cast<float>(column), flag.y + h * static_cast<float>(row), w, h},
                (row + column) % 2 == 0 ? style.color : Faded(style));
    p.Rect(flag, Outline(style.line * 0.75f, style.color, StrokeAlign::Inside));
    Segment(p, {pole, c.y - s}, {pole, c.y + s}, style.line * 1.4f, style.color);
}

// Feather: a diagonal vane, the shaft through it and barbs on both sides.
void Feather(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr float Widest = 0.55f;
    const Float2 dir = Dir(Radians(45.0f));
    const Float2 across = Perpendicular(dir);
    const Float2 base{c.x - s * 0.55f, c.y + s * 0.55f};
    const float length = s * 1.55f;
    const float width = s * 0.36f;
    p.Polygon(Leaf(base, dir, length, width, Widest), Framed(style));
    Segment(p, {c.x - s * 0.95f, c.y + s * 0.95f}, base + dir * length, style.line * 1.1f, style.color);
    for (const float t : {0.3f, 0.5f, 0.7f}) {
        const Float2 at = base + dir * (length * t);
        const float half = width * (t < Widest ? t / Widest : (1.0f - t) / (1.0f - Widest)) * 0.85f;
        for (const float side : {-1.0f, 1.0f})
            Segment(p, at, at + across * (side * half) - dir * (s * 0.15f), style.line * 0.7f, style.color);
    }
}

// Treasure chest: domed lid, banded body, a lock in the middle.
void Chest(Painter& p, Float2 c, float s, const IconStyle& style) {
    const FloatRect lid{c.x - s * 0.9f, c.y - s * 0.8f, s * 1.8f, s * 0.7f};
    const FloatRect body{c.x - s * 0.9f, c.y - s * 0.15f, s * 1.8f, s * 0.95f};
    p.RoundRect(lid, s * 0.21f, Framed(style));
    p.Rect(body, {.fill = Faded(style), .stroke = {style.line, style.color, StrokeAlign::Inside}});
    for (const float x : {-0.5f, 0.5f})
        Segment(p, {c.x + x * s, lid.y + s * 0.08f}, {c.x + x * s, body.Bottom()}, style.line, style.color);
    Box(p, {c.x - s * 0.16f, body.y - s * 0.16f, s * 0.32f, s * 0.4f}, style.color);
    Disc(p, {c.x, body.y + s * 0.02f}, s * 0.07f, DetailOr(style));
}

// Pickaxe: diagonal handle, a head curving to both sides at its end.
void Pickaxe(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 along{0.69f, -0.72f}; // Handle direction (bottom left to top right)
    const Float2 across{0.72f, 0.69f};
    const Float2 head{c.x + s * 0.3f, c.y - s * 0.3f};
    Segment(p, {c.x - s * 0.75f, c.y + s * 0.8f}, head, style.line * 1.6f, style.color);
    const auto at = [&](float a, float b) { return head + (along * a + across * b) * s; };
    const std::array points{at(-0.35f, -0.95f), at(0.0f, -0.55f), at(0.15f, 0.0f), at(0.0f, 0.55f), at(-0.35f, 0.95f)};
    Chain(p, points, style.line * 2.0f, style.color, StrokeJoin::Round, StrokeCap::Round);
}

// Anchor: ring on top, shank, stock and a lower arc with barbed ends.
void Anchor(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float w = style.line * 1.4f;
    const Float2 eye{c.x, c.y - s * 0.75f};
    Ring(p, eye, s * 0.2f, s * 0.2f + w, style.color);
    const Float2 arc{c.x, c.y + s * 0.1f};
    const float r = s * 0.72f;
    Segment(p, {c.x, eye.y + s * 0.2f}, {c.x, arc.y + r}, w, style.color);
    Segment(p, {c.x - s * 0.45f, c.y - s * 0.3f}, {c.x + s * 0.45f, c.y - s * 0.3f}, w, style.color);
    Band(p, arc, r - w, r, Radians(15.0f), Radians(165.0f), style.color);
    for (const float side : {-1.0f, 1.0f}) {
        const Float2 end = arc + Float2::FromAngle(Radians(side > 0.0f ? 15.0f : 165.0f), r - w * 0.5f);
        Tri(p, end + Float2{side * s * 0.1f, -s * 0.32f}, end + Float2{-side * s * 0.14f, s * 0.04f},
            end + Float2{side * s * 0.26f, s * 0.06f}, style.color);
    }
}

// Two crossed swords: pointed blade, grip, guard.
void Swords(Painter& p, Float2 c, float s, const IconStyle& style) {
    for (const float side : {-1.0f, 1.0f}) {
        const Float2 d{side * 0.707f, -0.707f};
        const Float2 across = Perpendicular(d);
        const Float2 pommel = c - d * (s * 1.15f);
        const auto at = [&](float along, float aside) { return pommel + (d * along + across * aside) * s; };
        const std::array blade{at(0.45f, 0.12f), at(1.95f, 0.12f), at(2.25f, 0.0f), at(1.95f, -0.12f),
                               at(0.45f, -0.12f)};
        p.Polygon(blade, Framed(style));
        Segment(p, at(0.42f, 0.35f), at(0.42f, -0.35f), style.line * 1.6f, style.color);
        Segment(p, at(0.0f, 0.0f), at(0.42f, 0.0f), style.line * 1.6f, style.color);
        Disc(p, at(0.0f, 0.0f), style.line * 1.3f, style.color);
    }
}

// Coiled snake: a spiral thickening outwards, the head with an eye at its outer end.
void Coil(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr int Segments = 40;
    constexpr float Turns = 2.1f;
    const float width = std::max(style.line * 1.6f, s * 0.2f);
    Float2 previous = c + Float2{s * 0.08f, 0.0f};
    for (int i = 1; i <= Segments; ++i) {
        const float t = static_cast<float>(i) / Segments;
        const Float2 point = c + Float2::FromAngle(t * Turns * 2.0f * Pi, s * (0.08f + 0.72f * t));
        Segment(p, previous, point, width * (0.45f + 0.55f * t), style.color, StrokeCap::Round);
        previous = point;
    }
    Disc(p, previous, s * 0.22f, style.color);
    Disc(p, previous + Float2{s * 0.05f, -s * 0.06f}, s * 0.07f, DetailOr(style));
}

// Swirl: three nested arcs at shifted angles and a centre.
void Swirl(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr std::array Radii{0.38f, 0.68f, 0.98f};
    Disc(p, c, s, Faded(style, FillAlpha * 0.6f));
    for (std::size_t k = 0; k < Radii.size(); ++k) {
        const float start = static_cast<float>(k) * 120.0f;
        Band(p, c, Radii[k] * s - style.line * 1.2f, Radii[k] * s, Radians(start), Radians(start + 230.0f),
             style.color);
    }
    Disc(p, c, s * 0.14f, style.color);
}

// Copy: two stacked pages; two visible edges of the back one.
void Copy(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 3> back{
        {{c.x - s * 0.9f, c.y + s * 0.45f}, {c.x - s * 0.9f, c.y - s * 0.9f}, {c.x + s * 0.45f, c.y - s * 0.9f}}};
    Chain(p, back, style.line, style.color);
    p.Rect({c.x - s * 0.45f, c.y - s * 0.45f, s * 1.3f, s * 1.4f},
           Outline(style.line, style.color, StrokeAlign::Inside));
}

// Cat's head: two pointed ears, a round face and slit eyes ('detail', dark without one).
void Cat(Painter& p, Float2 c, float s, const IconStyle& style) {
    Tri(p, {c.x - s * 0.88f, c.y - s}, {c.x - s * 0.78f, c.y - s * 0.05f}, {c.x - s * 0.12f, c.y - s * 0.5f},
        style.color);
    Tri(p, {c.x + s * 0.88f, c.y - s}, {c.x + s * 0.12f, c.y - s * 0.5f}, {c.x + s * 0.78f, c.y - s * 0.05f},
        style.color);
    Disc(p, {c.x, c.y + s * 0.18f}, s * 0.78f, style.color);
    const Color4 eye = style.detail.a > 0 ? style.detail : Tinted(Color4{10, 8, 20, 217}, style);
    for (const float side : {-1.0f, 1.0f}) {
        const Float2 at{c.x + side * s * 0.32f, c.y + s * 0.08f};
        p.Ellipse(at, {s * 0.16f, s * 0.24f}, Fill(eye));
        p.Ellipse(at, {s * 0.05f, s * 0.2f}, Fill(Faded(style, 0.9f)));
    }
}

// Couch: backrest, a two-cushion seat, round armrests and short legs.
void Couch(Painter& p, Float2 c, float s, const IconStyle& style) {
    p.RoundRect({c.x - s * 0.62f, c.y - s * 0.6f, s * 1.24f, s * 0.75f}, s * 0.15f, Fill(Faded(style, 0.7f)));
    p.RoundRect({c.x - s * 0.7f, c.y + s * 0.02f, s * 1.4f, s * 0.46f}, s * 0.08f, Fill(style.color));
    Segment(p, {c.x, c.y + s * 0.08f}, {c.x, c.y + s * 0.42f}, style.line, DetailOr(style));
    for (const float side : {-1.0f, 1.0f}) {
        p.RoundRect({c.x + side * s * 0.76f - s * 0.22f, c.y - s * 0.25f, s * 0.44f, s * 0.78f}, s * 0.13f,
                    Fill(style.color));
        Box(p, {c.x + side * s * 0.62f - s * 0.07f, c.y + s * 0.5f, s * 0.14f, s * 0.32f}, style.color);
    }
}

// Gas pedal: a tilted ribbed plate; below it a hinge and an arm resting on the floor.
void Pedal(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 top{c.x + s * 0.35f, c.y - s * 0.95f};
    const Float2 bottom{c.x - s * 0.35f, c.y + s * 0.55f};
    const Float2 along = bottom - top;
    const Float2 across = Perpendicular(Normalized(along)) * (s * 0.42f);
    const std::array<Float2, 4> plate{{top - across, top + across, bottom + across, bottom - across}};
    p.Polygon(plate, Fill(Faded(style, 0.75f)));
    for (int i = 1; i <= 3; ++i) {
        const Float2 at = top + along * (static_cast<float>(i) / 4.0f);
        Segment(p, at - across * 0.8f, at + across * 0.8f, style.line, DetailOr(style));
    }
    Disc(p, bottom, s * 0.16f, style.color);
    Segment(p, bottom, {c.x + s * 0.55f, c.y + s * 0.95f}, s * 0.18f, style.color);
    Segment(p, {c.x - s * 0.95f, c.y + s * 0.95f}, {c.x + s * 0.95f, c.y + s * 0.95f}, style.line, style.color);
}

// Mask: half smiling, half frowning; a vertical seam in the middle.
void Mask(Painter& p, Float2 c, float s, const IconStyle& style) {
    p.Ellipse(c, {s * 0.82f, s}, Fill(style.color));
    const Color4 hole = DetailOr(style);
    Segment(p, {c.x, c.y - s}, {c.x, c.y + s}, style.line, hole);
    for (const float side : {-1.0f, 1.0f})
        p.Ellipse({c.x + side * s * 0.36f, c.y - s * 0.22f}, {s * 0.17f, s * 0.12f}, Fill(hole));
    Band(p, {c.x - s * 0.02f, c.y + s * 0.1f}, s * 0.42f - style.line, s * 0.42f, Radians(90.0f), Radians(180.0f),
         hole);
    Band(p, {c.x + s * 0.02f, c.y + s * 0.75f}, s * 0.42f - style.line, s * 0.42f, Radians(270.0f), Radians(360.0f),
         hole);
}

// Egg silhouette for the icon set; the upper half is taller than the lower half.
void Egg(Painter& p, Float2 c, float s, Color4 color, Color4 detail) {
    constexpr std::size_t Segments = 48;
    constexpr float Width = 0.75f;
    const float top = (2.0f - Width) * s;
    const float equator = c.y + s - Width * s;
    std::array<Float2, Segments> outline{};
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const float angle = 2.0f * Pi * static_cast<float>(i) / static_cast<float>(Segments);
        const float y = std::sin(angle);
        outline[i] = {c.x + Width * s * std::cos(angle), equator + (y < 0.0f ? top : Width * s) * y};
    }
    p.Polygon(outline, Fill(color));
    p.Ellipse(c + Float2{-0.35f * Width * s, -0.35f * s}, {0.25f * Width * s, 0.3f * s}, Fill(detail));
}

// Hen: round body, head, comb, beak and an egg beside it.
void Hen(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 body{c.x - s * 0.2f, c.y + s * 0.2f};
    const Float2 head{c.x + s * 0.28f, c.y - s * 0.42f};
    Disc(p, body, s * 0.6f, style.color);
    Disc(p, head, s * 0.3f, style.color);
    Tri(p, head + Float2{-s * 0.2f, -s * 0.2f}, head + Float2{0.0f, -s * 0.55f}, head + Float2{s * 0.18f, -s * 0.2f},
        style.color);
    Tri(p, head + Float2{s * 0.26f, -s * 0.08f}, head + Float2{s * 0.58f, s * 0.02f},
        head + Float2{s * 0.26f, s * 0.12f}, style.color);
    Tri(p, body + Float2{-s * 0.5f, -s * 0.2f}, body + Float2{-s * 0.95f, -s * 0.6f},
        body + Float2{-s * 0.6f, s * 0.1f}, style.color);
    Disc(p, head + Float2{s * 0.06f, -s * 0.04f}, s * 0.06f, DetailOr(style));
    Egg(p, {c.x + s * 0.68f, c.y + s * 0.6f}, s * 0.35f, Faded(style, 0.8f), DetailOr(style));
}

// Studded tyre: thick ring, rim and studs around it.
void Tire(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr int Studs = 10;
    Ring(p, c, s * 0.5f, s * 0.86f, style.color);
    Ring(p, c, s * 0.22f, s * 0.3f, style.color);
    for (int i = 0; i < Studs; ++i) {
        const Float2 d = Dir(Radians(static_cast<float>(i) * (360.0f / Studs)));
        Disc(p, c + d * (s * 0.68f), s * 0.07f, DetailOr(style));
        Disc(p, c + d * (s * 0.93f), s * 0.07f, style.color);
    }
}

// Bubble: a big shiny one and two small ones that left it.
void Bubble(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 big{c.x + s * 0.18f, c.y - s * 0.15f};
    FramedDisc(p, big, s * 0.72f, style);
    Band(p, big, s * 0.42f, s * 0.42f + style.line, Radians(210.0f), Radians(290.0f), style.color);
    Ring(p, {c.x - s * 0.62f, c.y + s * 0.62f}, s * 0.24f - style.line, s * 0.24f, style.color);
    Disc(p, {c.x - s * 0.88f, c.y + s * 0.95f}, s * 0.1f, style.color);
}

// Scroll: a long parchment rolled at both ends, lines of writing on it.
void Scroll(Painter& p, Float2 c, float s, const IconStyle& style) {
    const FloatRect sheet{c.x - s * 0.62f, c.y - s * 0.8f, s * 1.24f, s * 1.6f};
    Box(p, sheet, Faded(style));
    Segment(p, {sheet.x, sheet.y}, {sheet.x, sheet.Bottom()}, style.line, style.color);
    Segment(p, {sheet.Right(), sheet.y}, {sheet.Right(), sheet.Bottom()}, style.line, style.color);
    for (const float y : {sheet.y, sheet.Bottom()})
        p.RoundRect({sheet.x - s * 0.18f, y - s * 0.14f, sheet.width + s * 0.36f, s * 0.28f}, s * 0.14f,
                    Fill(style.color));
    for (int i = 0; i < 3; ++i) {
        const float y = c.y - s * 0.35f + static_cast<float>(i) * s * 0.35f;
        Segment(p, {c.x - s * 0.38f, y}, {c.x + s * (i == 2 ? 0.1f : 0.38f), y}, style.line, style.color);
    }
}

// Flame: a drop, round below and pointed above; a small core in 'detail' inside.
void Flame(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 base{c.x, c.y + s * 0.38f};
    const float r = s * 0.6f;
    Disc(p, base, r, style.color);
    Tri(p, {c.x + s * 0.12f, c.y - s}, {base.x - r, base.y}, {base.x + r, base.y}, style.color);
    if (style.detail.a == 0) return;
    const Float2 core{c.x, c.y + s * 0.55f};
    const float coreRadius = s * 0.28f;
    Disc(p, core, coreRadius, style.detail);
    Tri(p, {c.x - s * 0.04f, c.y + s * 0.05f}, {core.x - coreRadius, core.y}, {core.x + coreRadius, core.y},
        style.detail);
}

// Gear: a holed body with teeth around it.
void Gear(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr int Teeth = 8;
    for (int i = 0; i < Teeth; ++i) {
        const Float2 dir = Float2::FromAngle(Radians(static_cast<float>(i) * (360.0f / Teeth)));
        Segment(p, c + dir * (s * 0.6f), c + dir * s, s * 0.3f, style.color);
    }
    Ring(p, c, s * 0.3f, s * 0.75f, style.color);
}

// Power button: a ring open at the top and a vertical line through the gap.
void Power(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float width = style.line * 1.6f;
    Band(p, c, s * 0.8f - width, s * 0.8f, Radians(-55.0f), Radians(235.0f), style.color);
    Segment(p, {c.x, c.y - s}, {c.x, c.y - s * 0.1f}, width, style.color, StrokeCap::Round);
}

// --- Interface icons ---

void Play(Painter& p, Float2 c, float s, Color4 color) {
    const std::array<Float2, 3> arrow{
        {{c.x - s * 0.62f, c.y - s * 0.9f}, {c.x + s * 0.92f, c.y}, {c.x - s * 0.62f, c.y + s * 0.9f}}};
    p.Polygon(arrow, {.fill = color, .stroke = {s * 0.12f, color, StrokeAlign::Center, StrokeJoin::Round}});
}

void Pause(Painter& p, Float2 c, float s, Color4 color) {
    for (const float side : {-1.0f, 1.0f})
        p.RoundRect({c.x + side * s * 0.42f - s * 0.25f, c.y - s * 0.88f, s * 0.5f, s * 1.76f}, s * 0.12f, Fill(color));
}

// A ring turning counter-clockwise, open at the top right, where the arrowhead ends it.
void Restart(Painter& p, Float2 c, float s, Color4 color) {
    const float radius = s * 0.64f;
    const Float2 center{c.x - s * 0.04f, c.y + s * 0.08f};
    p.Arc(center, radius, Radians(-115.0f), Radians(-280.0f),
          {.width = s * 0.28f, .color = color, .cap = StrokeCap::Round});
    const Float2 normal = Float2::FromAngle(Radians(-35.0f));
    const Float2 along{normal.y, -normal.x}; // Counter-clockwise on the ring
    const Float2 at = center + normal * radius;
    Tri(p, at + along * (s * 0.42f), at + normal * (s * 0.36f), at - normal * (s * 0.36f), color);
}

void Check(Painter& p, Float2 c, float s, Color4 color) {
    const std::array<Float2, 3> mark{
        {{c.x - s * 0.8f, c.y + s * 0.02f}, {c.x - s * 0.26f, c.y + s * 0.62f}, {c.x + s * 0.8f, c.y - s * 0.68f}}};
    Chain(p, mark, s * 0.3f, color, StrokeJoin::Round, StrokeCap::Round);
}

void Cross(Painter& p, Float2 c, float s, Color4 color) {
    const float d = s * 0.7f;
    Segment(p, {c.x - d, c.y - d}, {c.x + d, c.y + d}, s * 0.3f, color, StrokeCap::Round);
    Segment(p, {c.x + d, c.y - d}, {c.x - d, c.y + d}, s * 0.3f, color, StrokeCap::Round);
}

// House: a roof wider than its walls and a door.
void Home(Painter& p, Float2 c, float s, const IconStyle& style) {
    Tri(p, {c.x - s * 0.98f, c.y - s * 0.04f}, {c.x, c.y - s * 0.95f}, {c.x + s * 0.98f, c.y - s * 0.04f}, style.color);
    Box(p, {c.x - s * 0.68f, c.y - s * 0.1f, s * 1.36f, s * 1.02f}, style.color);
    p.RoundRect({c.x - s * 0.2f, c.y + s * 0.28f, s * 0.4f, s * 0.64f}, s * 0.06f, Fill(DetailOr(style)));
}

void Info(Painter& p, Float2 c, float s, const IconStyle& style) {
    FramedDisc(p, c, s * 0.98f, style);
    Disc(p, {c.x, c.y - s * 0.42f}, s * 0.15f, style.color);
    Segment(p, {c.x, c.y - s * 0.1f}, {c.x, c.y + s * 0.55f}, s * 0.24f, style.color, StrokeCap::Round);
}

// Rounded triangle with an exclamation mark.
void Warning(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 3> sign{
        {{c.x, c.y - s * 0.8f}, {c.x + s * 0.86f, c.y + s * 0.76f}, {c.x - s * 0.86f, c.y + s * 0.76f}}};
    p.Polygon(sign, {.fill = style.color, .stroke = {s * 0.2f, style.color, StrokeAlign::Center, StrokeJoin::Round}});
    const Color4 mark = DetailOr(style);
    Segment(p, {c.x, c.y - s * 0.28f}, {c.x, c.y + s * 0.26f}, s * 0.2f, mark, StrokeCap::Round);
    Disc(p, {c.x, c.y + s * 0.6f}, s * 0.12f, mark);
}

// Speaker facing right; what comes out of it is up to the icon.
void Speaker(Painter& p, Float2 c, float s, Color4 color) {
    const std::array<Float2, 6> cone{{{c.x - s * 0.95f, c.y - s * 0.32f},
                                      {c.x - s * 0.52f, c.y - s * 0.32f},
                                      {c.x - s * 0.06f, c.y - s * 0.78f},
                                      {c.x - s * 0.06f, c.y + s * 0.78f},
                                      {c.x - s * 0.52f, c.y + s * 0.32f},
                                      {c.x - s * 0.95f, c.y + s * 0.32f}}};
    p.Polygon(cone, Fill(color));
}

void SoundOn(Painter& p, Float2 c, float s, const IconStyle& style) {
    Speaker(p, c, s, style.color);
    for (const float radius : {0.4f, 0.8f})
        p.Arc({c.x + s * 0.06f, c.y}, s * radius, Radians(-48.0f), Radians(96.0f),
              {.width = s * 0.15f, .color = style.color, .cap = StrokeCap::Round});
}

void SoundOff(Painter& p, Float2 c, float s, const IconStyle& style) {
    Speaker(p, c, s, style.color);
    const Float2 at{c.x + s * 0.5f, c.y};
    const float d = s * 0.3f;
    const Color4 mark = AccentOr(style);
    Segment(p, at + Float2{-d, -d}, at + Float2{d, d}, s * 0.16f, mark, StrokeCap::Round);
    Segment(p, at + Float2{d, -d}, at + Float2{-d, d}, s * 0.16f, mark, StrokeCap::Round);
}

// Two notes joined by a beam.
void Music(Painter& p, Float2 c, float s, Color4 color) {
    const float stem = s * 0.14f;
    const Float2 head{s * 0.34f, s * 0.26f};
    const Float2 left{c.x - s * 0.56f, c.y + s * 0.66f};
    const Float2 right{c.x + s * 0.54f, c.y + s * 0.48f};
    const float leftStem = left.x + head.x - stem * 0.5f;
    const float rightStem = right.x + head.x - stem * 0.5f;
    p.Ellipse(left, head, Fill(color));
    p.Ellipse(right, head, Fill(color));
    Segment(p, {leftStem, left.y}, {leftStem, c.y - s * 0.5f}, stem, color);
    Segment(p, {rightStem, right.y}, {rightStem, c.y - s * 0.7f}, stem, color);
    const std::array<Float2, 4> beam{{{leftStem - stem * 0.5f, c.y - s * 0.68f},
                                      {rightStem + stem * 0.5f, c.y - s * 0.95f},
                                      {rightStem + stem * 0.5f, c.y - s * 0.6f},
                                      {leftStem - stem * 0.5f, c.y - s * 0.33f}}};
    p.Polygon(beam, Fill(color));
}

// Floppy disk: one corner cut, a shutter on top and a label below.
void Save(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float e = s * 0.92f;
    const std::array<Float2, 5> body{{{c.x - e, c.y - e},
                                      {c.x + s * 0.56f, c.y - e},
                                      {c.x + e, c.y - s * 0.56f},
                                      {c.x + e, c.y + e},
                                      {c.x - e, c.y + e}}};
    p.Polygon(body, Framed(style));
    Box(p, {c.x - s * 0.46f, c.y - e, s * 0.86f, s * 0.58f}, style.color);
    Box(p, {c.x + s * 0.06f, c.y - s * 0.82f, s * 0.2f, s * 0.32f}, DetailOr(style));
    Box(p, {c.x - s * 0.56f, c.y + s * 0.08f, s * 1.12f, s * 0.84f}, style.color);
    for (const float y : {0.32f, 0.6f})
        Segment(p, {c.x - s * 0.36f, c.y + s * y}, {c.x + s * 0.36f, c.y + s * y}, style.line * 0.8f, DetailOr(style));
}

// Bin: a tapered body with ribs, a lid and its handle.
void Trash(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 4> body{{{c.x - s * 0.66f, c.y - s * 0.46f},
                                      {c.x + s * 0.66f, c.y - s * 0.46f},
                                      {c.x + s * 0.5f, c.y + s * 0.94f},
                                      {c.x - s * 0.5f, c.y + s * 0.94f}}};
    p.Polygon(body, Framed(style));
    for (const float x : {-0.26f, 0.0f, 0.26f})
        Segment(p, {c.x + s * x, c.y - s * 0.2f}, {c.x + s * x * 0.82f, c.y + s * 0.7f}, style.line, style.color);
    const float lid = c.y - s * 0.62f;
    Segment(p, {c.x - s * 0.88f, lid}, {c.x + s * 0.88f, lid}, style.line * 1.6f, style.color, StrokeCap::Round);
    const std::array<Float2, 4> handle{{{c.x - s * 0.28f, lid},
                                        {c.x - s * 0.28f, c.y - s * 0.9f},
                                        {c.x + s * 0.28f, c.y - s * 0.9f},
                                        {c.x + s * 0.28f, lid}}};
    Chain(p, handle, style.line * 1.2f, style.color);
}

// --- Item and nature icons ---

// One sword on the diagonal: a framed blade with its ridge; guard and grip in 'detail'.
void Sword(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 d{0.707f, -0.707f};
    const Float2 across = Perpendicular(d);
    const Float2 pommel = c - d * (s * 1.2f);
    const auto at = [&](float along, float aside) { return pommel + (d * along + across * aside) * s; };
    const Color4 hilt = AccentOr(style);
    const std::array blade{at(0.6f, 0.17f), at(2.1f, 0.17f), at(2.48f, 0.0f), at(2.1f, -0.17f), at(0.6f, -0.17f)};
    p.Polygon(blade, Framed(style, 0.45f));
    Segment(p, at(0.65f, 0.0f), at(2.2f, 0.0f), style.line * 0.8f, style.color);
    Segment(p, at(0.56f, 0.46f), at(0.56f, -0.46f), s * 0.17f, hilt, StrokeCap::Round);
    Segment(p, at(0.08f, 0.0f), at(0.56f, 0.0f), s * 0.15f, hilt);
    Disc(p, at(0.08f, 0.0f), s * 0.14f, hilt);
}

// Upright bow with its string on the left; an arrow in 'detail' points right.
void Bow(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Color4 arrow = AccentOr(style);
    const Float2 center{c.x - s * 0.55f, c.y};
    const float width = s * 0.17f;
    const float radius = s - width * 0.5f;
    const float reach = Radians(68.0f);
    Segment(p, center + Float2::FromAngle(-reach, radius), center + Float2::FromAngle(reach, radius), style.line * 0.7f,
            arrow);
    p.Arc(center, radius, -reach, reach * 2.0f, {.width = width, .color = style.color, .cap = StrokeCap::Round});
    const float tip = c.x + s * 0.96f;
    Segment(p, {c.x - s * 0.8f, c.y}, {tip - s * 0.2f, c.y}, s * 0.08f, arrow);
    Tri(p, {tip, c.y}, {tip - s * 0.34f, c.y - s * 0.18f}, {tip - s * 0.34f, c.y + s * 0.18f}, arrow);
    for (const float side : {-1.0f, 1.0f})
        Segment(p, {c.x - s * 0.72f, c.y}, {c.x - s * 0.92f, c.y + side * s * 0.17f}, s * 0.08f, arrow);
}

// Round flask half full, a narrow neck and a cork in 'detail'.
void Potion(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 belly{c.x, c.y + s * 0.3f};
    const float r = s * 0.66f;
    HalfDisc(p, {belly.x, belly.y - s * 0.02f}, r - style.line * 0.5f, style.color);
    Box(p, {c.x - s * 0.2f, c.y - s * 0.66f, s * 0.4f, s * 0.42f}, Faded(style));
    FramedDisc(p, belly, r, style, 0.18f);
    for (const float side : {-1.0f, 1.0f})
        Segment(p, {c.x + side * s * 0.2f, c.y - s * 0.68f}, {c.x + side * s * 0.2f, c.y - s * 0.3f}, style.line,
                style.color);
    p.RoundRect({c.x - s * 0.27f, c.y - s * 0.98f, s * 0.54f, s * 0.36f}, s * 0.07f, Fill(AccentOr(style)));
    Disc(p, {c.x + s * 0.22f, belly.y - s * 0.24f}, s * 0.08f, Faded(style, 0.8f));
    Disc(p, {c.x - s * 0.18f, belly.y - s * 0.15f}, s * 0.05f, Faded(style, 0.8f));
}

// Round bomb: a cap, a curved fuse and a spark at its end in 'detail'.
void Bomb(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 body{c.x - s * 0.14f, c.y + s * 0.18f};
    const float r = s * 0.78f;
    const Float2 up = Normalized({1.0f, -1.0f});
    const Float2 cap = body + up * r;
    const std::array<Float2, 4> fuse{{cap,
                                      cap + Float2{s * 0.1f, -s * 0.2f},
                                      cap + Float2{s * 0.24f, -s * 0.3f},
                                      cap + Float2{s * 0.34f, -s * 0.22f}}};
    Chain(p, fuse, s * 0.09f, Tinted(Color4{220, 205, 170, 255}, style), StrokeJoin::Round);
    DrawRotatedRect(p, body + up * (r * 0.98f), {s * 0.2f, s * 0.13f}, Radians(-45.0f), Fill(style.color));
    Disc(p, body, r, style.color);
    p.Arc(body, r * 0.68f, Radians(195.0f), Radians(65.0f),
          {.width = s * 0.11f, .color = Tinted(WithAlpha(White, 0.65f), style), .cap = StrokeCap::Round});
    DrawStar(p, fuse.back(), s * 0.22f, Fill(AccentOr(style)));
}

// Skull: a round cranium over a jaw; sockets, nose and teeth in 'detail'.
void Skull(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Color4 hole = DetailOr(style);
    Disc(p, {c.x, c.y - s * 0.16f}, s * 0.82f, style.color);
    p.RoundRect({c.x - s * 0.44f, c.y + s * 0.28f, s * 0.88f, s * 0.68f}, s * 0.12f, Fill(style.color));
    for (const float side : {-1.0f, 1.0f})
        p.Ellipse({c.x + side * s * 0.34f, c.y - s * 0.1f}, {s * 0.21f, s * 0.24f}, Fill(hole));
    Tri(p, {c.x, c.y + s * 0.1f}, {c.x + s * 0.12f, c.y + s * 0.36f}, {c.x - s * 0.12f, c.y + s * 0.36f}, hole);
    for (const float x : {-0.15f, 0.15f})
        Segment(p, {c.x + s * x, c.y + s * 0.62f}, {c.x + s * x, c.y + s * 0.96f}, s * 0.08f, hole);
}

// Key on the diagonal: a round bow at the top left, two teeth at the end of the shaft.
void Key(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 d{0.707f, 0.707f};
    const Float2 across = Perpendicular(d);
    const Float2 eye{c.x - s * 0.5f, c.y - s * 0.5f};
    const float outer = s * 0.46f;
    const float width = s * 0.2f;
    Ring(p, eye, outer - width, outer, style.color);
    const auto at = [&](float along, float aside) { return eye + (d * along + across * aside) * s; };
    Segment(p, at(0.42f, 0.0f), at(1.8f, 0.0f), width, style.color);
    for (const float along : {1.32f, 1.7f})
        Segment(p, at(along, 0.0f), at(along, -0.3f), width * 0.9f, style.color);
}

// Crown: three points with a ball on each, a band below and gems in 'detail'.
void Crown(Painter& p, Float2 c, float s, const IconStyle& style) {
    const std::array<Float2, 7> points{{{c.x - s * 0.82f, c.y + s * 0.55f},
                                        {c.x - s * 0.88f, c.y - s * 0.52f},
                                        {c.x - s * 0.42f, c.y - s * 0.02f},
                                        {c.x, c.y - s * 0.74f},
                                        {c.x + s * 0.42f, c.y - s * 0.02f},
                                        {c.x + s * 0.88f, c.y - s * 0.52f},
                                        {c.x + s * 0.82f, c.y + s * 0.55f}}};
    p.Polygon(points, Fill(style.color));
    Disc(p, points[1], s * 0.11f, style.color);
    Disc(p, points[3], s * 0.12f, style.color);
    Disc(p, points[5], s * 0.11f, style.color);
    p.RoundRect({c.x - s * 0.82f, c.y + s * 0.62f, s * 1.64f, s * 0.26f}, s * 0.05f, Fill(style.color));
    const Color4 gem = DetailOr(style);
    Disc(p, {c.x, c.y + s * 0.28f}, s * 0.14f, gem);
    for (const float side : {-1.0f, 1.0f})
        Disc(p, {c.x + side * s * 0.5f, c.y + s * 0.3f}, s * 0.08f, gem);
}

// Cup with two handles on a stem and base; a star on the cup in 'detail'.
void Trophy(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float top = c.y - s * 0.92f;
    const float bottom = c.y - s * 0.3f;
    const float r = s * 0.56f;
    for (const float side : {-1.0f, 1.0f})
        p.Arc({c.x + side * r, c.y - s * 0.56f}, s * 0.27f, Radians(side > 0.0f ? -90.0f : 90.0f), Pi,
              {.width = s * 0.13f, .color = style.color});
    Box(p, {c.x - r, top, r * 2.0f, bottom - top + s * 0.04f}, style.color); // Overlaps the bowl: no seam
    HalfDisc(p, {c.x, bottom}, r, style.color);
    Box(p, {c.x - s * 0.1f, bottom + r * 0.8f, s * 0.2f, s * 0.5f}, style.color);
    p.RoundRect({c.x - s * 0.5f, c.y + s * 0.62f, s, s * 0.32f}, s * 0.06f, Fill(style.color));
    DrawStar(p, {c.x, c.y - s * 0.46f}, s * 0.3f, Fill(DetailOr(style)));
}

// Crescent leaning left, a small star in its hollow: a big circle less a smaller one set to the right.
void Moon(Painter& p, Float2 c, float s, Color4 color) {
    constexpr std::size_t Side = 24;
    constexpr float Outer = 0.92f;
    constexpr float Inner = 0.76f;
    constexpr float InnerAt = 0.62f;
    const float lean = Radians(-20.0f);
    const float tipX = (Outer * Outer - Inner * Inner + InnerAt * InnerAt) / (2.0f * InnerAt);
    const float tipY = std::sqrt(Outer * Outer - tipX * tipX);
    const float outerTip = std::atan2(tipY, tipX);
    const float innerTip = std::atan2(tipY, tipX - InnerAt);
    const auto place = [&](Float2 point) { return c + Rotated(point * s, lean); };
    std::array<Float2, Side * 2 - 2> crescent{};
    for (std::size_t i = 0; i < Side; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(Side - 1);
        crescent[i] = place(Float2::FromAngle(outerTip + (2.0f * Pi - 2.0f * outerTip) * t, Outer));
    }
    for (std::size_t i = 1; i + 1 < Side; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(Side - 1);
        const float angle = (2.0f * Pi - innerTip) - (2.0f * Pi - 2.0f * innerTip) * t;
        crescent[Side + i - 1] = place(Float2{InnerAt, 0.0f} + Float2::FromAngle(angle, Inner));
    }
    p.Polygon(crescent, Fill(color));
    DrawStar(p, place({InnerAt * 0.72f, 0.0f}), s * 0.24f, Fill(color));
}

// Drop: a circle closed by its tangents up to a point; a shine on the left.
void Droplet(Painter& p, Float2 c, float s, const IconStyle& style) {
    const Float2 belly{c.x, c.y + s * 0.3f};
    const float r = s * 0.66f;
    const float tip = s * 1.26f; // From the belly's centre up to the point
    const float along = r * r / tip;
    const float half = r * std::sqrt(1.0f - (r / tip) * (r / tip));
    Disc(p, belly, r, style.color);
    Tri(p, {c.x, belly.y - tip}, {c.x + half, belly.y - along}, {c.x - half, belly.y - along}, style.color);
    p.Arc(belly, r * 0.62f, Radians(110.0f), Radians(70.0f),
          {.width = s * 0.11f, .color = Tinted(WithAlpha(White, 0.7f), style), .cap = StrokeCap::Round});
}

// Six arms, each with a pair of branches.
void Snowflake(Painter& p, Float2 c, float s, const IconStyle& style) {
    const float width = std::max(style.line * 1.2f, s * 0.1f);
    for (int i = 0; i < 6; ++i) {
        const float angle = Radians(static_cast<float>(i) * 60.0f);
        const Float2 d = Dir(angle);
        Segment(p, c, c + d * (s * 0.93f), width, style.color, StrokeCap::Round);
        const Float2 fork = c + d * (s * 0.56f);
        for (const float side : {-1.0f, 1.0f})
            Segment(p, fork, fork + Dir(angle + side * Radians(55.0f)) * (s * 0.32f), width, style.color,
                    StrokeCap::Round);
    }
}

// A pointed leaf on the diagonal; rib and veins in 'detail', the stem in its own colour.
void LeafIcon(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr std::size_t Side = 16;
    const float width = s * 0.58f;
    const Float2 base{c.x - s * 0.62f, c.y + s * 0.62f};
    const Float2 along = Float2{c.x + s * 0.92f, c.y - s * 0.92f} - base;
    const Float2 across = Perpendicular(Normalized(along));
    std::array<Float2, Side * 2 - 2> outline{};
    for (std::size_t i = 0; i < Side; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(Side - 1);
        outline[i] = base + along * t + across * (width * std::sin(Pi * t));
        if (i > 0 && i + 1 < Side) outline[Side * 2 - 2 - i] = base + along * t - across * (width * std::sin(Pi * t));
    }
    p.Polygon(outline, Fill(style.color));
    const Color4 rib = DetailOr(style);
    Segment(p, {c.x - s * 0.9f, c.y + s * 0.9f}, base, s * 0.11f, style.color, StrokeCap::Round);
    Segment(p, base, base + along * 0.86f, s * 0.07f, rib);
    for (const float t : {0.3f, 0.55f})
        for (const float side : {-1.0f, 1.0f}) {
            const Float2 from = base + along * t;
            Segment(p, from, from + along * 0.14f + across * (side * width * 0.62f), s * 0.05f, rib);
        }
}

// Almond outline, an iris, a pupil in 'detail' and a glint.
void Eye(Painter& p, Float2 c, float s, const IconStyle& style) {
    constexpr std::size_t Side = 16;
    const float e = s - style.line * 0.5f;
    const float open = s * 0.62f;
    std::array<Float2, Side * 2 - 2> outline{};
    for (std::size_t i = 0; i < Side; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(Side - 1);
        const float x = c.x - e + 2.0f * e * t;
        outline[i] = {x, c.y - open * std::sin(Pi * t)};
        if (i > 0 && i + 1 < Side) outline[Side * 2 - 2 - i] = {x, c.y + open * std::sin(Pi * t)};
    }
    p.Polygon(outline, Framed(style));
    Disc(p, c, s * 0.42f, style.color);
    Disc(p, c, s * 0.2f, DetailOr(style));
    Disc(p, c + Float2{s * 0.12f, -s * 0.14f}, s * 0.08f, Tinted(White, style));
}

// Bullseye: two rings and a centre, faintly filled between them.
void Target(Painter& p, Float2 c, float s, const IconStyle& style) {
    Disc(p, c, s * 0.98f, Faded(style));
    Ring(p, c, s * 0.76f, s * 0.98f, style.color);
    Ring(p, c, s * 0.42f, s * 0.62f, style.color);
    Disc(p, c, s * 0.24f, style.color);
}
} // namespace

void DrawIcon(Painter& paint, Icon icon, Float2 center, float size, const IconLook& look) {
    if (icon == Icon::Count) return;
    const IconStyle style = Resolved(icon, size, look);
    Painter& p = paint;
    const Float2 c = center;
    const float s = size;
    switch (icon) {
    case Icon::Star       : DrawStar(p, c, s, Fill(style.color)); break;
    case Icon::Egg        : Egg(p, c, s, style.color, style.detail); break;
    case Icon::Hourglass  : Hourglass(p, c, s, style.color); break;
    case Icon::Plus       : Plus(p, c, s, style.color); break;
    case Icon::Minus      : Minus(p, c, s, style.color); break;
    case Icon::UpArrow    : UpArrow(p, c, s, style.color); break;
    case Icon::Chevrons   : Chevrons(p, c, s, style.color); break;
    case Icon::Apple      : Apple(p, c, s, style.color); break;
    case Icon::Magnet     : Magnet(p, c, s, style.color); break;
    case Icon::Ghost      : Ghost(p, c, s, style); break;
    case Icon::Brick      : Brick(p, c, s, style); break;
    case Icon::Rain       : Rain(p, c, s, style.color); break;
    case Icon::Clock      : Clock(p, c, s, style); break;
    case Icon::Book       : Book(p, c, s, style); break;
    case Icon::Shield     : Shield(p, c, s, style); break;
    case Icon::HornedEye  : HornedEye(p, c, s, style); break;
    case Icon::Sun        : Sun(p, c, s, style); break;
    case Icon::Gem        : Gem(p, c, s, style); break;
    case Icon::Bolt       : Bolt(p, c, s, style); break;
    case Icon::Rune       : Rune(p, c, s, style); break;
    case Icon::Mountain   : Mountain(p, c, s, style); break;
    case Icon::Lock       : Lock(p, c, s, style); break;
    case Icon::Coin       : Coin(p, c, s, style); break;
    case Icon::Dice       : Dice(p, c, s, style); break;
    case Icon::Cards      : Cards(p, c, s, style); break;
    case Icon::LoopArrow  : LoopArrow(p, c, s, style); break;
    case Icon::Cloud      : Cloud(p, c, s, style.color); break;
    case Icon::StormCloud : StormCloud(p, c, s, style); break;
    case Icon::Tornado    : Tornado(p, c, s, style); break;
    case Icon::CrownedStar: CrownedStar(p, c, s, style); break;
    case Icon::Compass    : Compass(p, c, s, style); break;
    case Icon::Comet      : Comet(p, c, s, style); break;
    case Icon::Gear       : Gear(p, c, s, style); break;
    case Icon::Heart      : Heart(p, c, s, style.color); break;
    case Icon::Power      : Power(p, c, s, style); break;
    case Icon::Lotus      : Lotus(p, c, s, style); break;
    case Icon::Goblet     : Goblet(p, c, s, style); break;
    case Icon::Stopwatch  : Stopwatch(p, c, s, style); break;
    case Icon::Sack       : Sack(p, c, s, style); break;
    case Icon::Wings      : Wings(p, c, s, style); break;
    case Icon::Flag       : Flag(p, c, s, style); break;
    case Icon::Feather    : Feather(p, c, s, style); break;
    case Icon::Chest      : Chest(p, c, s, style); break;
    case Icon::Pickaxe    : Pickaxe(p, c, s, style); break;
    case Icon::Anchor     : Anchor(p, c, s, style); break;
    case Icon::Swords     : Swords(p, c, s, style); break;
    case Icon::Coil       : Coil(p, c, s, style); break;
    case Icon::Clover     : Clover(p, c, s, style.color); break;
    case Icon::Swirl      : Swirl(p, c, s, style); break;
    case Icon::TurnArrow  : TurnArrow(p, c, s, style.color); break;
    case Icon::Copy       : Copy(p, c, s, style); break;
    case Icon::Cat        : Cat(p, c, s, style); break;
    case Icon::Couch      : Couch(p, c, s, style); break;
    case Icon::Pedal      : Pedal(p, c, s, style); break;
    case Icon::Mask       : Mask(p, c, s, style); break;
    case Icon::Hen        : Hen(p, c, s, style); break;
    case Icon::Tire       : Tire(p, c, s, style); break;
    case Icon::Bubble     : Bubble(p, c, s, style); break;
    case Icon::Scroll     : Scroll(p, c, s, style); break;
    case Icon::Flame      : Flame(p, c, s, style); break;
    case Icon::Play       : Play(p, c, s, style.color); break;
    case Icon::Pause      : Pause(p, c, s, style.color); break;
    case Icon::Restart    : Restart(p, c, s, style.color); break;
    case Icon::Check      : Check(p, c, s, style.color); break;
    case Icon::Cross      : Cross(p, c, s, style.color); break;
    case Icon::Home       : Home(p, c, s, style); break;
    case Icon::Info       : Info(p, c, s, style); break;
    case Icon::Warning    : Warning(p, c, s, style); break;
    case Icon::SoundOn    : SoundOn(p, c, s, style); break;
    case Icon::SoundOff   : SoundOff(p, c, s, style); break;
    case Icon::Music      : Music(p, c, s, style.color); break;
    case Icon::Save       : Save(p, c, s, style); break;
    case Icon::Trash      : Trash(p, c, s, style); break;
    case Icon::Sword      : Sword(p, c, s, style); break;
    case Icon::Bow        : Bow(p, c, s, style); break;
    case Icon::Potion     : Potion(p, c, s, style); break;
    case Icon::Bomb       : Bomb(p, c, s, style); break;
    case Icon::Skull      : Skull(p, c, s, style); break;
    case Icon::Key        : Key(p, c, s, style); break;
    case Icon::Crown      : Crown(p, c, s, style); break;
    case Icon::Trophy     : Trophy(p, c, s, style); break;
    case Icon::Moon       : Moon(p, c, s, style.color); break;
    case Icon::Droplet    : Droplet(p, c, s, style); break;
    case Icon::Snowflake  : Snowflake(p, c, s, style); break;
    case Icon::Leaf       : LeafIcon(p, c, s, style); break;
    case Icon::Eye        : Eye(p, c, s, style); break;
    case Icon::Target     : Target(p, c, s, style); break;
    case Icon::Count      : break;
    }
}
