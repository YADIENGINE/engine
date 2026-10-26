#include "Graphics/Icons/Icons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <utility>
#include "Graphics/Draw/DrawUtils.h"

namespace {
constexpr float Pi = 3.14159265f;
constexpr float Deg = Pi / 180.0f;
constexpr float FillAlpha = 0.3f; // Çizgi ikonlarının iç dolgusu

// --- Dolu ikonlar ---

void DrawHourglass(Vector2 c, float s, Color color) {
    const float w = s * 0.83f;
    FillTriangle({c.x - w, c.y - s}, c, {c.x + w, c.y - s}, color);
    FillTriangle(c, {c.x - w, c.y + s}, {c.x + w, c.y + s}, color);
}

void DrawPlus(Vector2 c, float s, Color color) {
    const float thick = s * 0.47f;
    DrawRectangleV({c.x - s, c.y - thick / 2.0f}, {s * 2.0f, thick}, color);
    DrawRectangleV({c.x - thick / 2.0f, c.y - s}, {thick, s * 2.0f}, color);
}

void DrawMinus(Vector2 c, float s, Color color) {
    const float thick = s * 0.47f;
    DrawRectangleV({c.x - s, c.y - thick / 2.0f}, {s * 2.0f, thick}, color);
}

void DrawUpArrow(Vector2 c, float s, Color color) {
    const float w = s * 0.92f;
    FillTriangle({c.x, c.y - s * 1.08f}, {c.x - w, c.y}, {c.x + w, c.y}, color);
    DrawRectangleV({c.x - s * 0.3f, c.y}, {s * 0.6f, s}, color);
}

void DrawChevrons(Vector2 c, float s, Color color) {
    const float thick = s * 0.33f;
    const float h = s * 0.83f;
    for (const float dx : {-s * 0.5f, s * 0.33f}) {
        DrawLineEx({c.x + dx - h * 0.5f, c.y - h}, {c.x + dx + h * 0.5f, c.y}, thick, color);
        DrawLineEx({c.x + dx + h * 0.5f, c.y}, {c.x + dx - h * 0.5f, c.y + h}, thick, color);
    }
}

void DrawApple(Vector2 c, float s, Color color) {
    DrawCircleV({c.x, c.y + s * 0.17f}, s * 0.75f, color);
    DrawRectangleV({c.x - s * 0.1f, c.y - s}, {s * 0.2f, s * 0.5f}, color);
}

void DrawMagnet(Vector2 c, float s, Color color) {
    const float inner = s * 0.5f;
    DrawRing(c, inner, s, 0.0f, 180.0f, 16, color);
    DrawRectangleV({c.x - s, c.y - s * 0.75f}, {s - inner, s * 0.75f}, color);
    DrawRectangleV({c.x + inner, c.y - s * 0.75f}, {s - inner, s * 0.75f}, color);
}

void DrawGhost(Vector2 c, float s, const IconStyle& style) {
    const float w = s * 0.83f;
    const float top = c.y - s * 0.17f;
    DrawCircleV({c.x, top}, w, style.color);
    DrawRectangleV({c.x - w, top}, {w * 2.0f, s * 0.92f}, style.color);
    DrawCircleV({c.x - w * 0.4f, top}, s * 0.2f, style.detail);
    DrawCircleV({c.x + w * 0.4f, top}, s * 0.2f, style.detail);
}

void DrawBrick(Vector2 c, float s, const IconStyle& style) {
    const float h = s * 0.75f;
    DrawRectangleV({c.x - s, c.y - h}, {s * 2.0f, h * 2.0f}, style.color);
    const float thick = s * 0.2f;
    DrawLineEx({c.x - s * 0.2f, c.y - h}, {c.x + s * 0.15f, c.y}, thick, style.detail);
    DrawLineEx({c.x + s * 0.15f, c.y}, {c.x - s * 0.1f, c.y + h}, thick, style.detail);
}

void DrawRain(Vector2 c, float s, Color color) {
    constexpr std::array<Vector2, 5> Drops{
        {{-0.58f, -0.5f}, {0.08f, -0.08f}, {0.67f, -0.58f}, {-0.33f, 0.58f}, {0.5f, 0.5f}}};
    for (const Vector2 drop : Drops) {
        DrawCircleV({c.x + drop.x * s, c.y + drop.y * s}, s * 0.28f, color);
    }
}

// Yuvarlak tepeli bulut; tabanı düz.
void DrawCloud(Vector2 c, float s, Color color) {
    DrawCircleV({c.x - s * 0.5f, c.y + s * 0.1f}, s * 0.4f, color);
    DrawCircleV({c.x + s * 0.05f, c.y - s * 0.15f}, s * 0.55f, color);
    DrawCircleV({c.x + s * 0.55f, c.y + s * 0.15f}, s * 0.38f, color);
    DrawRectangleRec({c.x - s * 0.5f, c.y + s * 0.1f, s * 1.05f, s * 0.43f}, color);
}

// Üç dişli taç takmış yıldız; taç 'detail' renginde (boşsa yıldızla aynı renk).
void DrawCrownedStar(Vector2 c, float s, const IconStyle& style) {
    DrawStar({c.x, c.y + s * 0.2f}, s * 0.78f, style.color);
    const Color crown = style.detail.a > 0 ? style.detail : style.color;
    const float base = c.y - s * 0.55f;
    const float notch = c.y - s * 0.75f;
    const float tip = c.y - s;
    const float w = s * 0.5f;
    DrawRectangleV({c.x - w, notch}, {w * 2.0f, base - notch}, crown);
    FillTriangle({c.x - w, tip}, {c.x - w, notch}, {c.x - w * 0.5f, notch}, crown);
    FillTriangle({c.x - w * 0.5f, notch}, {c.x, tip}, {c.x + w * 0.5f, notch}, crown);
    FillTriangle({c.x + w * 0.5f, notch}, {c.x + w, tip}, {c.x + w, notch}, crown);
}

// --- Çizgi ikonları ---

void DrawClock(Vector2 c, float s, const IconStyle& style) {
    DrawCircleV(c, s, Alpha(style.color, FillAlpha));
    DrawRing(c, s - style.line, s, 0.0f, 360.0f, 32, style.color);
    DrawLineEx(c, {c.x, c.y - s * 0.65f}, style.line * 1.4f, style.color);
    DrawLineEx(c, {c.x + s * 0.45f, c.y + s * 0.2f}, style.line * 1.4f, style.color);
    DrawCircleV(c, style.line * 1.4f, style.color);
}

void DrawBook(Vector2 c, float s, const IconStyle& style) {
    const Rectangle cover{c.x - s, c.y - s * 0.7f, s * 2.0f, s * 1.4f};
    DrawRectangleRounded(cover, 0.12f, 4, Alpha(style.color, FillAlpha));
    DrawRectangleRoundedLinesEx(cover, 0.12f, 4, style.line, style.color);
    DrawLineEx({c.x, cover.y}, {c.x, cover.y + cover.height}, style.line, style.color);
    for (int i = -1; i <= 1; ++i) {
        const float y = c.y + static_cast<float>(i) * s * 0.32f;
        DrawLineEx({c.x - s * 0.8f, y}, {c.x - s * 0.2f, y}, style.line * 0.67f, style.color);
        DrawLineEx({c.x + s * 0.2f, y}, {c.x + s * 0.8f, y}, style.line * 0.67f, style.color);
    }
}

void DrawShield(Vector2 c, float s, const IconStyle& style) {
    const std::array<Vector2, 5> points{{{c.x - s, c.y - s * 0.75f},
                                         {c.x + s, c.y - s * 0.75f},
                                         {c.x + s * 0.8f, c.y + s * 0.3f},
                                         {c.x, c.y + s},
                                         {c.x - s * 0.8f, c.y + s * 0.3f}}};
    for (std::size_t i = 0; i < points.size(); ++i) {
        FillTriangle(c, points[i], points[(i + 1) % points.size()], Alpha(style.color, FillAlpha));
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        DrawLineEx(points[i], points[(i + 1) % points.size()], style.line, style.color);
    }
    DrawLineEx({c.x, c.y - s * 0.45f}, {c.x, c.y + s * 0.5f}, style.line * 1.25f, style.color);
    DrawLineEx({c.x - s * 0.45f, c.y}, {c.x + s * 0.45f, c.y}, style.line * 1.25f, style.color);
}

void DrawHornedEye(Vector2 c, float s, const IconStyle& style) {
    FillTriangle({c.x - s * 0.85f, c.y - s}, {c.x - s * 0.65f, c.y - s * 0.15f}, {c.x - s * 0.2f, c.y - s * 0.5f},
                 style.color);
    FillTriangle({c.x + s * 0.85f, c.y - s}, {c.x + s * 0.2f, c.y - s * 0.5f}, {c.x + s * 0.65f, c.y - s * 0.15f},
                 style.color);
    DrawCircleV(c, s * 0.7f, Alpha(style.color, 0.25f));
    DrawRing(c, s * 0.7f - style.line, s * 0.7f, 0.0f, 360.0f, 32, style.color);
    DrawCircleV(c, s * 0.3f, style.color);
}

void DrawSun(Vector2 c, float s, const IconStyle& style) {
    DrawCircleV(c, s * 0.45f, style.color);
    for (int i = 0; i < 8; ++i) {
        const float angle = static_cast<float>(i) * 45.0f * Deg;
        const Vector2 dir{std::cos(angle), std::sin(angle)};
        DrawLineEx({c.x + dir.x * s * 0.6f, c.y + dir.y * s * 0.6f}, {c.x + dir.x * s, c.y + dir.y * s}, style.line,
                   style.color);
    }
}

// Bulut ve altından çakan şimşek.
void DrawStormCloud(Vector2 c, float s, const IconStyle& style) {
    DrawCloud({c.x, c.y - s * 0.35f}, s * 0.85f, style.color);
    const std::array points{Vector2{c.x + s * 0.15f, c.y + s * 0.05f}, Vector2{c.x - s * 0.2f, c.y + s * 0.55f},
                            Vector2{c.x + s * 0.15f, c.y + s * 0.5f}, Vector2{c.x - s * 0.1f, c.y + s}};
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        DrawLineEx(points[i], points[i + 1], style.line * 1.4f, style.color);
    }
}

// Aşağı doğru daralan ve yana kıvrılan hortum çizgileri.
void DrawTornado(Vector2 c, float s, const IconStyle& style) {
    constexpr int Lines = 5;
    for (int i = 0; i < Lines; ++i) {
        const float t = static_cast<float>(i) / (Lines - 1); // 0: tepe, 1: uç
        const float y = c.y + s * (-0.85f + 1.7f * t);
        const float half = s * (1.0f - 0.8f * t);
        const float x = c.x + s * 0.25f * std::sin(t * Pi * 1.2f);
        const float width = style.line * 1.4f;
        DrawLineEx({x - half, y}, {x + half, y}, width, style.color);
        DrawCircleV({x - half, y}, width / 2.0f, style.color);
        DrawCircleV({x + half, y}, width / 2.0f, style.color);
    }
}

// Sağ üstte parlak baş, sol alta doğru incelen kuyruk.
void DrawComet(Vector2 c, float s, const IconStyle& style) {
    const Vector2 head{c.x + s * 0.45f, c.y - s * 0.45f};
    constexpr int Streaks = 3;
    for (int i = 0; i < Streaks; ++i) {
        const float offset = (static_cast<float>(i) - 1.0f) * s * 0.28f;
        const Vector2 from{head.x + offset * 0.7f, head.y + offset * 0.7f};
        const Vector2 to{c.x - s * (i == 1 ? 1.0f : 0.7f) + offset * 0.7f,
                         c.y + s * (i == 1 ? 1.0f : 0.7f) + offset * 0.7f};
        DrawLineEx(from, to, style.line * (i == 1 ? 1.6f : 1.0f), Alpha(style.color, i == 1 ? 0.8f : 0.5f));
    }
    DrawCircleV(head, s * 0.42f, style.color);
    DrawCircleV({head.x - s * 0.12f, head.y - s * 0.12f}, s * 0.14f, Alpha(WHITE, 0.7f));
}

// Halka içinde iki uçlu ibre; kuzey ucu dolu, güney ucu soluk.
void DrawCompass(Vector2 c, float s, const IconStyle& style) {
    DrawCircleV(c, s, Alpha(style.color, FillAlpha));
    DrawRing(c, s - style.line, s, 0.0f, 360.0f, 32, style.color);
    const float w = s * 0.28f;
    const Vector2 north{c.x + s * 0.45f, c.y - s * 0.6f};
    const Vector2 south{c.x - s * 0.45f, c.y + s * 0.6f};
    const Vector2 left{c.x - w * 0.8f, c.y - w * 0.6f};
    const Vector2 right{c.x + w * 0.8f, c.y + w * 0.6f};
    FillTriangle(north, left, right, style.color);
    FillTriangle(south, left, right, Alpha(style.color, 0.55f));
    DrawCircleV(c, style.line, style.color);
}

void DrawGem(Vector2 c, float s, const IconStyle& style) {
    const Vector2 top{c.x, c.y - s};
    const Vector2 right{c.x + s * 0.75f, c.y - s * 0.25f};
    const Vector2 bottom{c.x, c.y + s};
    const Vector2 left{c.x - s * 0.75f, c.y - s * 0.25f};
    FillTriangle(top, left, bottom, Alpha(style.color, FillAlpha));
    FillTriangle(top, bottom, right, Alpha(style.color, FillAlpha));
    DrawLineEx(top, right, style.line, style.color);
    DrawLineEx(right, bottom, style.line, style.color);
    DrawLineEx(bottom, left, style.line, style.color);
    DrawLineEx(left, top, style.line, style.color);
    DrawLineEx(left, right, style.line, style.color);
}

void DrawBolt(Vector2 c, float s, const IconStyle& style) {
    const std::array points{Vector2{c.x + s * 0.3f, c.y - s}, Vector2{c.x - s * 0.4f, c.y + s * 0.05f},
                            Vector2{c.x + s * 0.35f, c.y - s * 0.05f}, Vector2{c.x - s * 0.3f, c.y + s}};
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        DrawLineEx(points[i], points[i + 1], style.line * 1.4f, style.color);
    }
    DrawCircleV({c.x + s * 0.7f, c.y + s * 0.6f}, style.line, style.color);
    DrawCircleV({c.x - s * 0.7f, c.y - s * 0.6f}, style.line, style.color);
}

void DrawRune(Vector2 c, float s, const IconStyle& style) {
    DrawRing(c, s - style.line, s, 0.0f, 360.0f, 32, style.color);
    std::array<Vector2, 3> points{};
    for (std::size_t k = 0; k < points.size(); ++k) {
        const float angle = (static_cast<float>(k) * 120.0f - 90.0f) * Deg;
        points[k] = {c.x + std::cos(angle) * s * 0.7f, c.y + std::sin(angle) * s * 0.7f};
    }
    for (std::size_t k = 0; k < points.size(); ++k) {
        DrawLineEx(points[k], points[(k + 1) % points.size()], style.line, style.color);
    }
    DrawCircleV(c, style.line * 1.2f, style.color);
}

void DrawMountain(Vector2 c, float s, const IconStyle& style) {
    const Vector2 baseLeft{c.x - s, c.y + s * 0.6f};
    const Vector2 baseRight{c.x + s, c.y + s * 0.6f};
    const Vector2 peak{c.x - s * 0.2f, c.y - s * 0.8f};
    const Vector2 peak2{c.x + s * 0.5f, c.y - s * 0.2f};
    FillTriangle(baseLeft, peak, {c.x + s * 0.35f, baseLeft.y}, Alpha(style.color, FillAlpha));
    DrawLineEx(baseLeft, peak, style.line, style.color);
    DrawLineEx(peak, {c.x + s * 0.2f, c.y + s * 0.05f}, style.line, style.color);
    DrawLineEx({c.x + s * 0.1f, c.y + s * 0.15f}, peak2, style.line, style.color);
    DrawLineEx(peak2, baseRight, style.line, style.color);
    DrawLineEx(baseLeft, baseRight, style.line, style.color);
}

// Asma kilit: yarım halka kanca, gövde, anahtar deliği.
void DrawLock(Vector2 c, float s, const IconStyle& style) {
    const Rectangle body{c.x - s * 0.8f, c.y - s * 0.1f, s * 1.6f, s * 1.1f};
    DrawRing({c.x, body.y}, s * 0.45f, s * 0.45f + style.line * 1.3f, 180.0f, 360.0f, 24, style.color);
    DrawLineEx({c.x - s * 0.45f - style.line * 0.65f, body.y}, {c.x - s * 0.45f - style.line * 0.65f, body.y + 1.0f},
               style.line * 1.3f, style.color);
    DrawRectangleRounded(body, 0.2f, 4, Alpha(style.color, FillAlpha));
    DrawRectangleRoundedLinesEx(body, 0.2f, 4, style.line, style.color);
    const Vector2 hole{c.x, body.y + body.height * 0.42f};
    DrawCircleV(hole, s * 0.16f, style.color);
    DrawLineEx(hole, {hole.x, hole.y + s * 0.3f}, style.line * 1.2f, style.color);
}

// Para: çift kenarlı daire, ortada yıldız.
void DrawCoin(Vector2 c, float s, const IconStyle& style) {
    DrawCircleV(c, s, Alpha(style.color, FillAlpha));
    DrawRing(c, s - style.line, s, 0.0f, 360.0f, 32, style.color);
    DrawRing(c, s * 0.72f - style.line * 0.5f, s * 0.72f, 0.0f, 360.0f, 32, style.color);
    DrawStar(c, s * 0.42f, style.color);
}

// Zar: yuvarlak köşeli kare, beşli yüz.
void DrawDice(Vector2 c, float s, const IconStyle& style) {
    const Rectangle body{c.x - s * 0.85f, c.y - s * 0.85f, s * 1.7f, s * 1.7f};
    DrawRectangleRounded(body, 0.3f, 6, Alpha(style.color, FillAlpha));
    DrawRectangleRoundedLinesEx(body, 0.3f, 6, style.line, style.color);
    const float pip = s * 0.16f;
    const float d = s * 0.45f;
    for (const Vector2 offset : {Vector2{-d, -d}, Vector2{d, -d}, Vector2{0.0f, 0.0f}, Vector2{-d, d}, Vector2{d, d}}) {
        DrawCircleV({c.x + offset.x, c.y + offset.y}, pip, style.color);
    }
}

// Alt kenarının ortasındaki 'pivot' etrafında 'degrees' dönmüş dikdörtgenin köşeleri (sol üstten saat yönünde).
std::array<Vector2, 4> RotatedCard(Vector2 pivot, float width, float height, float degrees) {
    const float a = degrees * Deg;
    const auto at = [&](float x, float y) {
        return Vector2{pivot.x + x * std::cos(a) - y * std::sin(a), pivot.y + x * std::sin(a) + y * std::cos(a)};
    };
    return {at(-width / 2.0f, -height), at(width / 2.0f, -height), at(width / 2.0f, 0.0f), at(-width / 2.0f, 0.0f)};
}

// Kartlar: alttan yelpaze gibi açılmış üç kart; öndekinin ortasında baklava.
void DrawCards(Vector2 c, float s, const IconStyle& style) {
    const Vector2 pivot{c.x, c.y + s * 0.85f};
    const float width = s * 0.95f;
    const float height = s * 1.55f;
    for (const float degrees : {-24.0f, 24.0f, 0.0f}) {
        const std::array<Vector2, 4> card = RotatedCard(pivot, width, height, degrees);
        const bool front = degrees == 0.0f;
        FillTriangle(card[0], card[1], card[2], front ? Fade(BLACK, 0.55f) : Alpha(style.color, FillAlpha));
        FillTriangle(card[0], card[2], card[3], front ? Fade(BLACK, 0.55f) : Alpha(style.color, FillAlpha));
        if (front) {
            FillTriangle(card[0], card[1], card[2], Alpha(style.color, FillAlpha));
            FillTriangle(card[0], card[2], card[3], Alpha(style.color, FillAlpha));
        }
        for (std::size_t k = 0; k < card.size(); ++k) {
            DrawLineEx(card[k], card[(k + 1) % card.size()], style.line, style.color);
        }
    }
    const Vector2 mid{c.x, pivot.y - height * 0.5f};
    const float d = s * 0.22f;
    FillTriangle({mid.x, mid.y - d * 1.4f}, {mid.x - d, mid.y}, {mid.x + d, mid.y}, style.color);
    FillTriangle({mid.x - d, mid.y}, {mid.x, mid.y + d * 1.4f}, {mid.x + d, mid.y}, style.color);
}

// Dönen ok: sağda açık bir halka, ucunda ok başı.
void DrawLoopArrow(Vector2 c, float s, const IconStyle& style) {
    constexpr float Start = 40.0f;
    constexpr float End = 300.0f;
    const float radius = s * 0.8f;
    DrawCircleV(c, radius, Alpha(style.color, FillAlpha * 0.6f));
    DrawRing(c, radius - style.line, radius, Start, End, 32, style.color);
    // Ok başı halkanın bittiği yerde, dönüş yönüne bakar.
    const float a = End * Deg;
    const Vector2 tip{c.x + std::cos(a) * (radius - style.line / 2.0f),
                      c.y + std::sin(a) * (radius - style.line / 2.0f)};
    const Vector2 tangent{-std::sin(a), std::cos(a)};
    const Vector2 normal{std::cos(a), std::sin(a)};
    const float head = s * 0.42f;
    FillTriangle({tip.x + tangent.x * head, tip.y + tangent.y * head},
                 {tip.x + normal.x * head * 0.65f, tip.y + normal.y * head * 0.65f},
                 {tip.x - normal.x * head * 0.65f, tip.y - normal.y * head * 0.65f}, style.color);
    DrawCircleV(c, style.line * 1.3f, style.color);
}
// --- Başarım ikonları ---

// 0° yukarı, saat yönünde artan açının birim yönü.
Vector2 Dir(float degrees) { return {std::sin(degrees * Deg), -std::cos(degrees * Deg)}; }

// Kökten 'dir' yönünde uzanan yaprak (baklava); en geniş yeri boyunun 'widest' oranında.
std::array<Vector2, 4> Leaf(Vector2 base, Vector2 dir, float length, float width, float widest) {
    const Vector2 perp{-dir.y, dir.x};
    const Vector2 mid{base.x + dir.x * length * widest, base.y + dir.y * length * widest};
    return {base,
            {mid.x + perp.x * width, mid.y + perp.y * width},
            {base.x + dir.x * length, base.y + dir.y * length},
            {mid.x - perp.x * width, mid.y - perp.y * width}};
}

// Dışbükey çokgen: soluk dolgu + çizgi.
void Outlined(std::span<const Vector2> points, const IconStyle& style) {
    Vector2 mid{0.0f, 0.0f};
    for (const Vector2 point : points) {
        mid = {mid.x + point.x, mid.y + point.y};
    }
    const auto count = static_cast<float>(points.size());
    mid = {mid.x / count, mid.y / count};
    for (std::size_t i = 0; i < points.size(); ++i) {
        FillTriangle(mid, points[i], points[(i + 1) % points.size()], Alpha(style.color, FillAlpha));
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        DrawLineEx(points[i], points[(i + 1) % points.size()], style.line, style.color);
    }
}

// Uçları yuvarlak kalın çizgi zinciri.
void Polyline(std::span<const Vector2> points, float width, Color color) {
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0) DrawLineEx(points[i - 1], points[i], width, color);
        DrawCircleV(points[i], width / 2.0f, color);
    }
}

// İkonun içindeki koyu ayrıntı (göz, anahtar deliği): 'detail' boşsa yarı saydam siyah.
Color DetailOr(const IconStyle& style) { return style.detail.a > 0 ? style.detail : Fade(BLACK, 0.6f); }

// Nilüfer: alttan açılan beş yaprak, altında su çizgisi.
void DrawLotus(Vector2 c, float s, const IconStyle& style) {
    const Vector2 base{c.x, c.y + s * 0.6f};
    constexpr std::array<std::pair<float, float>, 5> Petals{
        {{-72.0f, 0.85f}, {72.0f, 0.85f}, {-36.0f, 1.2f}, {36.0f, 1.2f}, {0.0f, 1.5f}}};
    for (const auto& [degrees, length] : Petals) {
        Outlined(Leaf(base, Dir(degrees), s * length, s * 0.24f, 0.5f), style);
    }
    DrawLineEx({c.x - s, c.y + s * 0.82f}, {c.x + s, c.y + s * 0.82f}, style.line, style.color);
}

// Kadeh: yarım daire hazne (içi dolu), ince ayak, düz taban; kenarında parıltı.
void DrawGoblet(Vector2 c, float s, const IconStyle& style) {
    const Vector2 bowl{c.x, c.y - s * 0.45f};
    const float r = s * 0.75f;
    DrawCircleSector(bowl, r, 0.0f, 180.0f, 24, Alpha(style.color, FillAlpha));
    DrawCircleSector({bowl.x, bowl.y + s * 0.12f}, r * 0.72f, 0.0f, 180.0f, 24, Alpha(style.color, 0.65f));
    DrawRing(bowl, r - style.line, r, 0.0f, 180.0f, 24, style.color);
    DrawLineEx({bowl.x - r, bowl.y}, {bowl.x + r, bowl.y}, style.line, style.color);
    DrawLineEx({c.x, bowl.y + r}, {c.x, c.y + s * 0.82f}, style.line * 1.3f, style.color);
    DrawLineEx({c.x - s * 0.5f, c.y + s * 0.85f}, {c.x + s * 0.5f, c.y + s * 0.85f}, style.line * 1.5f, style.color);
    DrawStar({c.x + s * 0.45f, bowl.y - s * 0.38f}, s * 0.22f, style.color);
}

// Kronometre: kadran, tepede düğme, yanda ikinci düğme, çeyrek işaretleri ve ibre.
void DrawStopwatch(Vector2 c, float s, const IconStyle& style) {
    const Vector2 face{c.x, c.y + s * 0.15f};
    const float r = s * 0.82f;
    const auto onFace = [&](Vector2 dir, float radius) {
        return Vector2{face.x + dir.x * radius, face.y + dir.y * radius};
    };
    DrawCircleV(face, r, Alpha(style.color, FillAlpha));
    DrawRing(face, r - style.line, r, 0.0f, 360.0f, 32, style.color);
    DrawRectangleRec({c.x - s * 0.2f, c.y - s, s * 0.4f, s * 0.18f}, style.color);
    DrawLineEx({c.x, c.y - s * 0.84f}, {c.x, face.y - r}, style.line * 1.4f, style.color);
    DrawLineEx(onFace(Dir(45.0f), r), onFace(Dir(45.0f), r + s * 0.16f), style.line * 1.6f, style.color);
    for (int i = 0; i < 4; ++i) {
        const Vector2 d = Dir(90.0f * static_cast<float>(i));
        DrawLineEx(onFace(d, r * 0.6f), onFace(d, r * 0.78f), style.line, style.color);
    }
    DrawLineEx(face, onFace(Dir(50.0f), r * 0.62f), style.line * 1.4f, style.color);
    DrawCircleV(face, style.line * 1.3f, style.color);
}

// Para kesesi: yuvarlak gövde üstünde yıldız, bağlı boğaz, açılan ağız.
void DrawSack(Vector2 c, float s, const IconStyle& style) {
    const Vector2 body{c.x, c.y + s * 0.28f};
    const float r = s * 0.7f;
    DrawCircleV(body, r, Alpha(style.color, FillAlpha));
    DrawRing(body, r - style.line, r, 0.0f, 360.0f, 32, style.color);
    const float neck = c.y - s * 0.5f;
    Outlined(
        std::array<Vector2, 4>{
            {{c.x - s * 0.22f, neck}, {c.x - s * 0.55f, c.y - s}, {c.x + s * 0.55f, c.y - s}, {c.x + s * 0.22f, neck}}},
        style);
    for (const float side : {-1.0f, 1.0f}) {
        DrawLineEx({c.x + side * s * 0.22f, neck}, {c.x + side * s * 0.3f, body.y - r * 0.9f}, style.line, style.color);
    }
    DrawLineEx({c.x - s * 0.34f, neck}, {c.x + s * 0.34f, neck}, style.line * 1.8f, style.color);
    DrawStar({body.x, body.y + s * 0.02f}, s * 0.32f, style.color);
}

// Kanatlar: iki yana açılan üçer tüy, ortada yukarı bakan uç.
void DrawWings(Vector2 c, float s, const IconStyle& style) {
    constexpr std::array<std::pair<float, float>, 3> Feathers{{{78.0f, 0.88f}, {52.0f, 0.95f}, {26.0f, 0.8f}}};
    const Vector2 root{c.x, c.y + s * 0.45f};
    for (const float side : {-1.0f, 1.0f}) {
        const Vector2 from{root.x + side * s * 0.12f, root.y};
        for (const auto& [degrees, length] : Feathers) {
            Outlined(Leaf(from, Dir(side * degrees), s * length, s * 0.15f, 0.6f), style);
        }
    }
    DrawCircleV(root, s * 0.16f, style.color);
    FillTriangle({c.x, c.y - s}, {c.x - s * 0.24f, c.y - s * 0.58f}, {c.x + s * 0.24f, c.y - s * 0.58f}, style.color);
}

// Bitiş bayrağı: direk ve dama desenli bez.
void DrawFlag(Vector2 c, float s, const IconStyle& style) {
    constexpr int Columns = 4;
    constexpr int Rows = 3;
    const float pole = c.x - s * 0.75f;
    const Rectangle flag{pole, c.y - s * 0.9f, s * 1.6f, s * 1.05f};
    const float w = flag.width / Columns;
    const float h = flag.height / Rows;
    for (int row = 0; row < Rows; ++row) {
        for (int column = 0; column < Columns; ++column) {
            DrawRectangleRec({flag.x + w * static_cast<float>(column), flag.y + h * static_cast<float>(row), w, h},
                             (row + column) % 2 == 0 ? style.color : Alpha(style.color, FillAlpha));
        }
    }
    DrawRectangleLinesEx(flag, style.line * 0.75f, style.color);
    DrawLineEx({pole, c.y - s}, {pole, c.y + s}, style.line * 1.4f, style.color);
}

// Tüy: çapraz duran bayrak, ortasından geçen sap ve iki yana çentikler.
void DrawFeather(Vector2 c, float s, const IconStyle& style) {
    constexpr float Widest = 0.55f;
    const Vector2 dir = Dir(45.0f);
    const Vector2 perp{-dir.y, dir.x};
    const Vector2 base{c.x - s * 0.55f, c.y + s * 0.55f};
    const float length = s * 1.55f;
    const float width = s * 0.36f;
    Outlined(Leaf(base, dir, length, width, Widest), style);
    DrawLineEx({c.x - s * 0.95f, c.y + s * 0.95f}, {base.x + dir.x * length, base.y + dir.y * length},
               style.line * 1.1f, style.color);
    for (const float t : {0.3f, 0.5f, 0.7f}) {
        const Vector2 at{base.x + dir.x * length * t, base.y + dir.y * length * t};
        const float half = width * (t < Widest ? t / Widest : (1.0f - t) / (1.0f - Widest)) * 0.85f;
        for (const float side : {-1.0f, 1.0f}) {
            const Vector2 to{at.x + perp.x * side * half - dir.x * s * 0.15f,
                             at.y + perp.y * side * half - dir.y * s * 0.15f};
            DrawLineEx(at, to, style.line * 0.7f, style.color);
        }
    }
}

// Hazine sandığı: kubbeli kapak, kuşaklı gövde, ortada kilit.
void DrawChest(Vector2 c, float s, const IconStyle& style) {
    const Rectangle lid{c.x - s * 0.9f, c.y - s * 0.8f, s * 1.8f, s * 0.7f};
    const Rectangle body{c.x - s * 0.9f, c.y - s * 0.15f, s * 1.8f, s * 0.95f};
    DrawRectangleRounded(lid, 0.6f, 6, Alpha(style.color, FillAlpha));
    DrawRectangleRoundedLinesEx(lid, 0.6f, 6, style.line, style.color);
    DrawRectangleRec(body, Alpha(style.color, FillAlpha));
    DrawRectangleLinesEx(body, style.line, style.color);
    for (const float x : {-0.5f, 0.5f}) {
        DrawLineEx({c.x + x * s, lid.y + s * 0.08f}, {c.x + x * s, body.y + body.height}, style.line, style.color);
    }
    DrawRectangleRec({c.x - s * 0.16f, body.y - s * 0.16f, s * 0.32f, s * 0.4f}, style.color);
    DrawCircleV({c.x, body.y + s * 0.02f}, s * 0.07f, DetailOr(style));
}

// Kazma: çapraz sap, ucunda iki yana kıvrılan baş.
void DrawPickaxe(Vector2 c, float s, const IconStyle& style) {
    const Vector2 d{0.69f, -0.72f}; // Sap yönü (sol alttan sağ üste)
    const Vector2 p{0.72f, 0.69f};
    const Vector2 head{c.x + s * 0.3f, c.y - s * 0.3f};
    DrawLineEx({c.x - s * 0.75f, c.y + s * 0.8f}, head, style.line * 1.6f, style.color);
    const auto at = [&](float along, float across) {
        return Vector2{head.x + (d.x * along + p.x * across) * s, head.y + (d.y * along + p.y * across) * s};
    };
    const std::array points{at(-0.35f, -0.95f), at(0.0f, -0.55f), at(0.15f, 0.0f), at(0.0f, 0.55f), at(-0.35f, 0.95f)};
    Polyline(points, style.line * 2.0f, style.color);
}

// Çapa: tepede halka, gövde, kol ve iki ucu oklu alt yay.
void DrawAnchor(Vector2 c, float s, const IconStyle& style) {
    const float w = style.line * 1.4f;
    const Vector2 eye{c.x, c.y - s * 0.75f};
    DrawRing(eye, s * 0.2f, s * 0.2f + w, 0.0f, 360.0f, 24, style.color);
    const Vector2 arc{c.x, c.y + s * 0.1f};
    const float r = s * 0.72f;
    DrawLineEx({c.x, eye.y + s * 0.2f}, {c.x, arc.y + r}, w, style.color);
    DrawLineEx({c.x - s * 0.45f, c.y - s * 0.3f}, {c.x + s * 0.45f, c.y - s * 0.3f}, w, style.color);
    DrawRing(arc, r - w, r, 15.0f, 165.0f, 24, style.color);
    for (const float side : {-1.0f, 1.0f}) {
        const float a = (side > 0.0f ? 15.0f : 165.0f) * Deg;
        const Vector2 end{arc.x + std::cos(a) * (r - w / 2.0f), arc.y + std::sin(a) * (r - w / 2.0f)};
        FillTriangle({end.x + side * s * 0.1f, end.y - s * 0.32f}, {end.x - side * s * 0.14f, end.y + s * 0.04f},
                     {end.x + side * s * 0.26f, end.y + s * 0.06f}, style.color);
    }
}

// Çapraz iki kılıç: sivri namlu, kabza, balçak.
void DrawSwords(Vector2 c, float s, const IconStyle& style) {
    for (const float side : {-1.0f, 1.0f}) {
        const Vector2 d{side * 0.707f, -0.707f};
        const Vector2 p{-d.y, d.x};
        const Vector2 pommel{c.x - d.x * s * 1.15f, c.y - d.y * s * 1.15f};
        const auto at = [&](float along, float across) {
            return Vector2{pommel.x + (d.x * along + p.x * across) * s, pommel.y + (d.y * along + p.y * across) * s};
        };
        Outlined(std::array{at(0.45f, 0.12f), at(1.95f, 0.12f), at(2.25f, 0.0f), at(1.95f, -0.12f), at(0.45f, -0.12f)},
                 style);
        DrawLineEx(at(0.42f, 0.35f), at(0.42f, -0.35f), style.line * 1.6f, style.color);
        DrawLineEx(at(0.0f, 0.0f), at(0.42f, 0.0f), style.line * 1.6f, style.color);
        DrawCircleV(at(0.0f, 0.0f), style.line * 1.3f, style.color);
    }
}

// Kıvrılmış yılan: içten dışa kalınlaşan sarmal, dış ucunda gözlü baş.
void DrawCoil(Vector2 c, float s, const IconStyle& style) {
    constexpr int Segments = 40;
    constexpr float Turns = 2.1f;
    const float width = std::max(style.line * 1.6f, s * 0.2f);
    std::array<Vector2, Segments + 1> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float t = static_cast<float>(i) / Segments;
        const float a = t * Turns * 2.0f * Pi;
        const float r = s * (0.08f + 0.72f * t);
        points[i] = {c.x + std::cos(a) * r, c.y + std::sin(a) * r};
    }
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float thick = width * (0.45f + 0.55f * static_cast<float>(i) / Segments);
        DrawLineEx(points[i - 1], points[i], thick, style.color);
        DrawCircleV(points[i], thick / 2.0f, style.color);
    }
    const Vector2 head = points.back();
    DrawCircleV(head, s * 0.22f, style.color);
    DrawCircleV({head.x + s * 0.05f, head.y - s * 0.06f}, s * 0.07f, DetailOr(style));
}

// Dört yapraklı yonca: kalp biçimli dört yaprak ve sap.
void DrawClover(Vector2 c, float s, Color color) {
    const Vector2 mid{c.x, c.y - s * 0.1f};
    DrawLineEx(mid, {c.x + s * 0.35f, c.y + s}, s * 0.14f, color);
    for (int i = 0; i < 4; ++i) {
        const Vector2 d = Dir(45.0f + 90.0f * static_cast<float>(i));
        const Vector2 p{-d.y, d.x};
        const Vector2 leaf{mid.x + d.x * s * 0.42f, mid.y + d.y * s * 0.42f};
        for (const float side : {-1.0f, 1.0f}) {
            DrawCircleV({leaf.x + p.x * side * s * 0.17f, leaf.y + p.y * side * s * 0.17f}, s * 0.26f, color);
        }
        FillTriangle(mid, {leaf.x + p.x * s * 0.36f, leaf.y + p.y * s * 0.36f},
                     {leaf.x - p.x * s * 0.36f, leaf.y - p.y * s * 0.36f}, color);
    }
}

// Girdap: kaymış açılarla iç içe üç yay ve merkez.
void DrawSwirl(Vector2 c, float s, const IconStyle& style) {
    constexpr std::array Radii{0.38f, 0.68f, 0.98f};
    DrawCircleV(c, s, Alpha(style.color, FillAlpha * 0.6f));
    for (std::size_t k = 0; k < Radii.size(); ++k) {
        const float start = static_cast<float>(k) * 120.0f;
        DrawRing(c, Radii[k] * s - style.line * 1.2f, Radii[k] * s, start, start + 230.0f, 32, style.color);
    }
    DrawCircleV(c, s * 0.14f, style.color);
}

// Sağa dönüş oku: soldan yukarı çıkan gövde, çeyrek kavisle sağa kıvrılır, ucunda ok başı.
void DrawTurnArrow(Vector2 c, float s, Color color) {
    const float thick = s * 0.36f;
    const Vector2 bend{c.x - s * 0.05f, c.y - s * 0.1f};
    const float outer = s * 0.68f;
    const float mid = outer - thick / 2.0f;
    DrawRectangleV({bend.x - outer, bend.y}, {thick, c.y + s - bend.y}, color);
    DrawRing(bend, outer - thick, outer, 180.0f, 270.0f, 16, color);
    const float barY = bend.y - outer;
    const float headX = c.x + s * 0.35f;
    DrawRectangleV({bend.x, barY}, {headX - bend.x, thick}, color);
    const float tipY = bend.y - mid;
    FillTriangle({c.x + s * 0.98f, tipY}, {headX, tipY - s * 0.42f}, {headX, tipY + s * 0.42f}, color);
}
// Kopyalama: üst üste iki sayfa; arka sayfanın görünen iki kenarı.
void DrawCopy(Vector2 c, float s, const IconStyle& style) {
    const Rectangle front{c.x - s * 0.45f, c.y - s * 0.45f, s * 1.3f, s * 1.4f};
    DrawLineEx({c.x - s * 0.9f, c.y + s * 0.45f}, {c.x - s * 0.9f, c.y - s * 0.9f}, style.line, style.color);
    DrawLineEx({c.x - s * 0.9f, c.y - s * 0.9f}, {c.x + s * 0.45f, c.y - s * 0.9f}, style.line, style.color);
    DrawRectangleLinesEx(front, style.line, style.color);
}
// Kedi başı: sivri iki kulak, yuvarlak yüz ve dikey yarık gözler (detail; yoksa koyu).
void DrawCat(Vector2 c, float s, const IconStyle& style) {
    FillTriangle({c.x - s * 0.88f, c.y - s}, {c.x - s * 0.78f, c.y - s * 0.05f}, {c.x - s * 0.12f, c.y - s * 0.5f},
                 style.color);
    FillTriangle({c.x + s * 0.88f, c.y - s}, {c.x + s * 0.12f, c.y - s * 0.5f}, {c.x + s * 0.78f, c.y - s * 0.05f},
                 style.color);
    DrawCircleV({c.x, c.y + s * 0.18f}, s * 0.78f, style.color);
    const Color eye = style.detail.a > 0 ? style.detail : Alpha(Color{10, 8, 20, 255}, 0.85f);
    for (const float side : {-1.0f, 1.0f}) {
        const Vector2 at{c.x + side * s * 0.32f, c.y + s * 0.08f};
        DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y), s * 0.16f, s * 0.24f, eye);
        DrawEllipse(static_cast<int>(at.x), static_cast<int>(at.y), s * 0.05f, s * 0.2f, Alpha(style.color, 0.9f));
    }
}
// Kanepe: arkalık, iki minderli oturak, yuvarlak kolçaklar ve kısa ayaklar.
void DrawCouch(Vector2 c, float s, const IconStyle& style) {
    DrawRectangleRounded({c.x - s * 0.62f, c.y - s * 0.6f, s * 1.24f, s * 0.75f}, 0.4f, 6, Alpha(style.color, 0.7f));
    DrawRectangleRounded({c.x - s * 0.7f, c.y + s * 0.02f, s * 1.4f, s * 0.46f}, 0.35f, 6, style.color);
    DrawLineEx({c.x, c.y + s * 0.08f}, {c.x, c.y + s * 0.42f}, style.line, DetailOr(style));
    for (const float side : {-1.0f, 1.0f}) {
        DrawRectangleRounded({c.x + side * s * 0.76f - s * 0.22f, c.y - s * 0.25f, s * 0.44f, s * 0.78f}, 0.6f, 6,
                             style.color);
        DrawRectangleV({c.x + side * s * 0.62f - s * 0.07f, c.y + s * 0.5f}, {s * 0.14f, s * 0.32f}, style.color);
    }
}
// Gaz pedalı: yatık, kaburgalı plaka; altında menteşe ve zemine yaslanan kol.
void DrawPedal(Vector2 c, float s, const IconStyle& style) {
    const Vector2 top{c.x + s * 0.35f, c.y - s * 0.95f};
    const Vector2 bottom{c.x - s * 0.35f, c.y + s * 0.55f};
    const Vector2 along{bottom.x - top.x, bottom.y - top.y};
    const float length = std::sqrt(along.x * along.x + along.y * along.y);
    const Vector2 across{-along.y / length * s * 0.42f, along.x / length * s * 0.42f};
    FillQuad({top.x - across.x, top.y - across.y}, {top.x + across.x, top.y + across.y},
             {bottom.x + across.x, bottom.y + across.y}, {bottom.x - across.x, bottom.y - across.y},
             Alpha(style.color, 0.75f));
    for (int i = 1; i <= 3; ++i) {
        const float t = static_cast<float>(i) / 4.0f;
        const Vector2 at{top.x + along.x * t, top.y + along.y * t};
        DrawLineEx({at.x - across.x * 0.8f, at.y - across.y * 0.8f}, {at.x + across.x * 0.8f, at.y + across.y * 0.8f},
                   style.line, DetailOr(style));
    }
    DrawCircleV(bottom, s * 0.16f, style.color);
    DrawLineEx(bottom, {c.x + s * 0.55f, c.y + s * 0.95f}, s * 0.18f, style.color);
    DrawLineEx({c.x - s * 0.95f, c.y + s * 0.95f}, {c.x + s * 0.95f, c.y + s * 0.95f}, style.line, style.color);
}
// Maske: yüzün yarısı gülen, yarısı somurtan; ortadan dikey ek yeri.
void DrawMask(Vector2 c, float s, const IconStyle& style) {
    DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), s * 0.82f, s, style.color);
    const Color hole = DetailOr(style);
    DrawLineEx({c.x, c.y - s}, {c.x, c.y + s}, style.line, hole);
    for (const float side : {-1.0f, 1.0f}) {
        DrawEllipse(static_cast<int>(c.x + side * s * 0.36f), static_cast<int>(c.y - s * 0.22f), s * 0.17f, s * 0.12f,
                    hole);
    }
    // Sol yarı gülümser, sağ yarı somurtur.
    DrawRing({c.x - s * 0.02f, c.y + s * 0.1f}, s * 0.42f - style.line, s * 0.42f, 90.0f, 180.0f, 12, hole);
    DrawRing({c.x + s * 0.02f, c.y + s * 0.75f}, s * 0.42f - style.line, s * 0.42f, 270.0f, 360.0f, 12, hole);
}
// Tavuk: yuvarlak gövde, baş, ibik, gaga ve yanında yumurta.
void DrawHen(Vector2 c, float s, const IconStyle& style) {
    const Vector2 body{c.x - s * 0.2f, c.y + s * 0.2f};
    const Vector2 head{c.x + s * 0.28f, c.y - s * 0.42f};
    DrawCircleV(body, s * 0.6f, style.color);
    DrawCircleV(head, s * 0.3f, style.color);
    FillTriangle({head.x - s * 0.2f, head.y - s * 0.2f}, {head.x, head.y - s * 0.55f},
                 {head.x + s * 0.18f, head.y - s * 0.2f}, style.color);
    FillTriangle({head.x + s * 0.26f, head.y - s * 0.08f}, {head.x + s * 0.58f, head.y + s * 0.02f},
                 {head.x + s * 0.26f, head.y + s * 0.12f}, style.color);
    FillTriangle({body.x - s * 0.5f, body.y - s * 0.2f}, {body.x - s * 0.95f, body.y - s * 0.6f},
                 {body.x - s * 0.6f, body.y + s * 0.1f}, style.color);
    DrawCircleV({head.x + s * 0.06f, head.y - s * 0.04f}, s * 0.06f, DetailOr(style));
    DrawEgg({c.x + s * 0.68f, c.y + s * 0.6f}, s * 0.7f, Alpha(style.color, 0.8f), DetailOr(style));
}
// Çivili lastik: kalın halka, jant ve çevresinde çiviler.
void DrawTire(Vector2 c, float s, const IconStyle& style) {
    constexpr int Studs = 10;
    DrawRing(c, s * 0.5f, s * 0.86f, 0.0f, 360.0f, 32, style.color);
    DrawRing(c, s * 0.22f, s * 0.3f, 0.0f, 360.0f, 24, style.color);
    for (int i = 0; i < Studs; ++i) {
        const Vector2 d = Dir(static_cast<float>(i) * (360.0f / Studs));
        DrawCircleV({c.x + d.x * s * 0.68f, c.y + d.y * s * 0.68f}, s * 0.07f, DetailOr(style));
        DrawCircleV({c.x + d.x * s * 0.93f, c.y + d.y * s * 0.93f}, s * 0.07f, style.color);
    }
}
// Balon: büyük, parlak bir kabarcık ve ondan çıkmış iki küçük.
void DrawBubble(Vector2 c, float s, const IconStyle& style) {
    const Vector2 big{c.x + s * 0.18f, c.y - s * 0.15f};
    DrawCircleV(big, s * 0.72f, Alpha(style.color, FillAlpha));
    DrawRing(big, s * 0.72f - style.line, s * 0.72f, 0.0f, 360.0f, 32, style.color);
    DrawRing(big, s * 0.42f, s * 0.42f + style.line, 210.0f, 290.0f, 12, style.color);
    DrawRing({c.x - s * 0.62f, c.y + s * 0.62f}, s * 0.24f - style.line, s * 0.24f, 0.0f, 360.0f, 20, style.color);
    DrawCircleV({c.x - s * 0.88f, c.y + s * 0.95f}, s * 0.1f, style.color);
}
// Tomar: iki ucu sarılı uzun parşömen ve üstünde yazı satırları.
void DrawScroll(Vector2 c, float s, const IconStyle& style) {
    const Rectangle sheet{c.x - s * 0.62f, c.y - s * 0.8f, s * 1.24f, s * 1.6f};
    DrawRectangleRec(sheet, Alpha(style.color, FillAlpha));
    DrawLineEx({sheet.x, sheet.y}, {sheet.x, sheet.y + sheet.height}, style.line, style.color);
    DrawLineEx({sheet.x + sheet.width, sheet.y}, {sheet.x + sheet.width, sheet.y + sheet.height}, style.line,
               style.color);
    for (const float y : {sheet.y, sheet.y + sheet.height}) {
        DrawRectangleRounded({sheet.x - s * 0.18f, y - s * 0.14f, sheet.width + s * 0.36f, s * 0.28f}, 1.0f, 8,
                             style.color);
    }
    for (int i = 0; i < 3; ++i) {
        const float y = c.y - s * 0.35f + static_cast<float>(i) * s * 0.35f;
        DrawLineEx({c.x - s * 0.38f, y}, {c.x + s * (i == 2 ? 0.1f : 0.38f), y}, style.line, style.color);
    }
}

// Alev: altı yuvarlak, ucu yukarıda damla; içinde ayrıntı renginde küçük çekirdek.
void DrawFlame(Vector2 c, float s, const IconStyle& style) {
    const Vector2 base{c.x, c.y + s * 0.38f};
    const float r = s * 0.6f;
    DrawCircleV(base, r, style.color);
    FillTriangle({c.x + s * 0.12f, c.y - s}, {base.x - r, base.y}, {base.x + r, base.y}, style.color);
    if (style.detail.a == 0) return;
    const Vector2 core{c.x, c.y + s * 0.55f};
    const float coreRadius = s * 0.28f;
    DrawCircleV(core, coreRadius, style.detail);
    FillTriangle({c.x - s * 0.04f, c.y + s * 0.05f}, {core.x - coreRadius, core.y}, {core.x + coreRadius, core.y},
                 style.detail);
}
} // namespace

// Dişli: delikli gövde, çevresinde dişler.
void DrawGear(Vector2 c, float s, const IconStyle& style) {
    constexpr int Teeth = 8;
    for (int i = 0; i < Teeth; ++i) {
        const float angle = static_cast<float>(i) * (360.0f / Teeth) * Deg;
        const Vector2 dir{std::cos(angle), std::sin(angle)};
        DrawLineEx({c.x + dir.x * s * 0.6f, c.y + dir.y * s * 0.6f}, {c.x + dir.x * s, c.y + dir.y * s}, s * 0.3f,
                   style.color);
    }
    DrawRing(c, s * 0.3f, s * 0.75f, 0.0f, 360.0f, 32, style.color);
}

// Kalp: iki daire ve sivri alt uç.
void DrawHeart(Vector2 c, float s, Color color) {
    const float radius = s * 0.52f;
    DrawCircleV({c.x - s * 0.48f, c.y - s * 0.3f}, radius, color);
    DrawCircleV({c.x + s * 0.48f, c.y - s * 0.3f}, radius, color);
    FillTriangle({c.x - s * 0.96f, c.y - s * 0.12f}, {c.x + s * 0.96f, c.y - s * 0.12f}, {c.x, c.y + s * 0.95f}, color);
}

// Güç düğmesi: üstü açık halka ve ortasında dik çizgi.
void DrawPower(Vector2 c, float s, const IconStyle& style) {
    const float width = style.line * 1.6f;
    DrawRing(c, s * 0.8f - width, s * 0.8f, -55.0f, 235.0f, 32, style.color);
    DrawLineEx({c.x, c.y - s}, {c.x, c.y - s * 0.1f}, width, style.color);
    DrawCircleV({c.x, c.y - s}, width / 2.0f, style.color);
    DrawCircleV({c.x, c.y - s * 0.1f}, width / 2.0f, style.color);
}

void DrawIcon(Icon icon, Vector2 center, float size, const IconStyle& style) {
    switch (icon) {
    case Icon::Star       : DrawStar(center, size, style.color); break;
    case Icon::Egg        : DrawEgg(center, size * 2.0f, style.color, style.detail); break;
    case Icon::Hourglass  : DrawHourglass(center, size, style.color); break;
    case Icon::Plus       : DrawPlus(center, size, style.color); break;
    case Icon::Minus      : DrawMinus(center, size, style.color); break;
    case Icon::UpArrow    : DrawUpArrow(center, size, style.color); break;
    case Icon::Chevrons   : DrawChevrons(center, size, style.color); break;
    case Icon::Apple      : DrawApple(center, size, style.color); break;
    case Icon::Magnet     : DrawMagnet(center, size, style.color); break;
    case Icon::Ghost      : DrawGhost(center, size, style); break;
    case Icon::Brick      : DrawBrick(center, size, style); break;
    case Icon::Rain       : DrawRain(center, size, style.color); break;
    case Icon::Clock      : DrawClock(center, size, style); break;
    case Icon::Book       : DrawBook(center, size, style); break;
    case Icon::Shield     : DrawShield(center, size, style); break;
    case Icon::HornedEye  : DrawHornedEye(center, size, style); break;
    case Icon::Sun        : DrawSun(center, size, style); break;
    case Icon::Gem        : DrawGem(center, size, style); break;
    case Icon::Bolt       : DrawBolt(center, size, style); break;
    case Icon::Rune       : DrawRune(center, size, style); break;
    case Icon::Mountain   : DrawMountain(center, size, style); break;
    case Icon::Lock       : DrawLock(center, size, style); break;
    case Icon::Coin       : DrawCoin(center, size, style); break;
    case Icon::Dice       : DrawDice(center, size, style); break;
    case Icon::Cards      : DrawCards(center, size, style); break;
    case Icon::LoopArrow  : DrawLoopArrow(center, size, style); break;
    case Icon::Cloud      : DrawCloud(center, size, style.color); break;
    case Icon::StormCloud : DrawStormCloud(center, size, style); break;
    case Icon::Tornado    : DrawTornado(center, size, style); break;
    case Icon::CrownedStar: DrawCrownedStar(center, size, style); break;
    case Icon::Compass    : DrawCompass(center, size, style); break;
    case Icon::Comet      : DrawComet(center, size, style); break;
    case Icon::Gear       : DrawGear(center, size, style); break;
    case Icon::Heart      : DrawHeart(center, size, style.color); break;
    case Icon::Power      : DrawPower(center, size, style); break;
    case Icon::Lotus      : DrawLotus(center, size, style); break;
    case Icon::Goblet     : DrawGoblet(center, size, style); break;
    case Icon::Stopwatch  : DrawStopwatch(center, size, style); break;
    case Icon::Sack       : DrawSack(center, size, style); break;
    case Icon::Wings      : DrawWings(center, size, style); break;
    case Icon::Flag       : DrawFlag(center, size, style); break;
    case Icon::Feather    : DrawFeather(center, size, style); break;
    case Icon::Chest      : DrawChest(center, size, style); break;
    case Icon::Pickaxe    : DrawPickaxe(center, size, style); break;
    case Icon::Anchor     : DrawAnchor(center, size, style); break;
    case Icon::Swords     : DrawSwords(center, size, style); break;
    case Icon::Coil       : DrawCoil(center, size, style); break;
    case Icon::Clover     : DrawClover(center, size, style.color); break;
    case Icon::Swirl      : DrawSwirl(center, size, style); break;
    case Icon::TurnArrow  : DrawTurnArrow(center, size, style.color); break;
    case Icon::Copy       : DrawCopy(center, size, style); break;
    case Icon::Cat        : DrawCat(center, size, style); break;
    case Icon::Couch      : DrawCouch(center, size, style); break;
    case Icon::Pedal      : DrawPedal(center, size, style); break;
    case Icon::Mask       : DrawMask(center, size, style); break;
    case Icon::Hen        : DrawHen(center, size, style); break;
    case Icon::Tire       : DrawTire(center, size, style); break;
    case Icon::Bubble     : DrawBubble(center, size, style); break;
    case Icon::Scroll     : DrawScroll(center, size, style); break;
    case Icon::Flame      : DrawFlame(center, size, style); break;
    case Icon::Count      : break;
    }
}
