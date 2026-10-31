#include "Graphics/Icons/Icons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/FloatRect.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr float FillAlpha = 0.3f; // Inner fill of line icons
constexpr Color4 Black{0, 0, 0, 255};
constexpr Color4 White{255, 255, 255, 255};
constexpr std::size_t HalfDiscPoints = 25;

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
} // namespace

void DrawIcon(Painter& paint, Icon icon, Float2 center, float size, const IconStyle& style) {
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
    case Icon::Count      : break;
    }
}
