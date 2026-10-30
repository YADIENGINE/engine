#include "Graphics/Draw/DrawUtils.h"
#include <rlgl.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/EffectQuality.h"
#include "Graphics/Render/Internals/RenderTargetStack.h"
#include "Graphics/Render/Internals/RenderExecutionContext.h"

namespace {
constexpr float StarInnerRatio = 0.45f;
constexpr int StarPoints = 5;

// Çokgeni merkezden yelpaze olarak çizer. raylib'in arka yüz kırpması ekranda saat yönünün tersini (y aşağı,
// shoelace toplamı negatif) bekler; ters sarımlı çokgen sondan başa dolaşılır. 'uvs' boşsa düz renk.
void DrawFan(unsigned textureId, std::span<const Vector2> points, std::span<const Vector2> uvs, Color color) {
    const std::size_t count = uvs.empty() ? points.size() : std::min(points.size(), uvs.size());
    if (count < 3) return;
    float area = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const Vector2 a = points[i];
        const Vector2 b = points[(i + 1) % count];
        area += a.x * b.y - b.x * a.y;
    }
    const auto at = [&](std::size_t i) { return area > 0.0f ? count - 1 - i : i; };
    const auto vertex = [&](std::size_t i) {
        if (uvs.empty())
            rlTexCoord2f(0.0f, 0.0f);
        else
            rlTexCoord2f(uvs[at(i)].x, uvs[at(i)].y);
        rlVertex2f(points[at(i)].x, points[at(i)].y);
    };
    // Her yelpaze dilimi tek köşesi tekrarlanan bir dörtgendir (raylib'in dokulu çokgen yolu).
    rlCheckRenderBatchLimit(static_cast<int>(4 * (count - 2)));
    rlSetTexture(textureId);
    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    for (std::size_t i = 1; i + 1 < count; ++i) {
        vertex(0);
        vertex(i);
        vertex(i + 1);
        vertex(i + 1);
    }
    rlEnd();
    rlSetTexture(0);
}
} // namespace

void FillRect(const Rect& rect, Color color) { DrawRectangle(rect.x, rect.y, rect.width, rect.height, color); }
void BeginClip(int x, int y, int width, int height) {
    BeginTargetScissor(
        {static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)});
}
Vector2 RectCenter(const Rect& rect) {
    return {static_cast<float>(rect.CenterX()), static_cast<float>(rect.CenterY())};
}
Vector2 RectCenter(Rectangle rect) { return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f}; }
Rectangle Inset(Rectangle rect, float amount) {
    const float x = std::min(amount, rect.width / 2.0f);
    const float y = std::min(amount, rect.height / 2.0f);
    return {rect.x + x, rect.y + y, rect.width - 2.0f * x, rect.height - 2.0f * y};
}

void DrawTabOutline(Rectangle tab, float thickness, Color color) {
    const float right = tab.x + tab.width;
    const float bottom = tab.y + tab.height;
    DrawLineEx({tab.x, bottom}, {tab.x, tab.y}, thickness, color);
    DrawLineEx({tab.x, tab.y}, {right, tab.y}, thickness, color);
    DrawLineEx({right, tab.y}, {right, bottom}, thickness, color);
}
void DrawTabbedPanelOutline(Rectangle panel, Rectangle activeTab, float thickness, Color color) {
    const float right = panel.x + panel.width;
    const float bottom = panel.y + panel.height;
    DrawLineEx({panel.x, panel.y}, {panel.x, bottom}, thickness, color);
    DrawLineEx({panel.x, bottom}, {right, bottom}, thickness, color);
    DrawLineEx({right, bottom}, {right, panel.y}, thickness, color);
    DrawLineEx({panel.x, panel.y}, {activeTab.x, panel.y}, thickness, color);
    DrawLineEx({activeTab.x + activeTab.width, panel.y}, {right, panel.y}, thickness, color);
}

void DrawStar(Vector2 center, float outerRadius, Color color) {
    constexpr int cornerCount = StarPoints * 2;
    std::array<Vector2, cornerCount> corners{};
    for (int i = 0; i < cornerCount; ++i) {
        const float radius = i % 2 == 0 ? outerRadius : outerRadius * StarInnerRatio;
        const float angle = -PI / 2.0f + static_cast<float>(i) * PI / StarPoints;
        corners[static_cast<std::size_t>(i)] = {center.x + radius * std::cos(angle),
                                                center.y + radius * std::sin(angle)};
    }

    for (std::size_t i = 0; i < corners.size(); ++i) {
        DrawTriangle(center, corners[(i + 1) % corners.size()], corners[i], color);
    }
}

// Dikey elips + alt yarıyı dolduran daire + parlama.
void DrawEgg(Vector2 center, float height, Color color, Color shine) {
    const float radiusV = height / 2.0f;
    const float radiusH = radiusV * 0.75f;
    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y), radiusH, radiusV, color);
    DrawCircleV({center.x, center.y + radiusV - radiusH}, radiusH, color);
    DrawEllipse(static_cast<int>(center.x - radiusH * 0.35f), static_cast<int>(center.y - radiusV * 0.35f),
                radiusH * 0.25f, radiusV * 0.3f, shine);
}

void DrawGlow(Vector2 center, float radius, Color color) {
    if (CurrentEffectQuality().glow == EffectLevel::Off) return;
    RenderExecutionContext::Count(RenderCounter::Glows, 1);
    RenderExecutionContext::Count(RenderCounter::GlowBlendRequests, 2);
    BeginBlendMode(BLEND_ADDITIVE);
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), radius, color, Fade(color, 0.0f));
    EndBlendMode();
}

void FillPolygon(std::span<const Vector2> points, Color color) { DrawFan(rlGetTextureIdDefault(), points, {}, color); }

void DrawTexturedPolygon(const Texture2D& texture, std::span<const Vector2> points, std::span<const Vector2> uvs,
                         Color tint) {
    if (uvs.empty()) return;
    DrawFan(texture.id, points, uvs, tint);
}

Rectangle ToRectangle(const Rect& rect) {
    return {static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width),
            static_cast<float>(rect.height)};
}

Color Alpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(static_cast<float>(color.a) * Clamp01(alpha));
    return color;
}

Vector2 Mix(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

void FillTriangle(Vector2 a, Vector2 b, Vector2 c, Color color) {
    const float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f)
        DrawTriangle(a, c, b, color);
    else
        DrawTriangle(a, b, c, color);
}

void FillQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
    FillTriangle(a, b, c, color);
    FillTriangle(a, c, d, color);
}

void DrawDiamond(Vector2 center, float halfWidth, float halfHeight, Color color) {
    FillQuad({center.x, center.y - halfHeight}, {center.x + halfWidth, center.y}, {center.x, center.y + halfHeight},
             {center.x - halfWidth, center.y}, color);
}

void DrawDiamondLines(Vector2 center, float halfWidth, float halfHeight, float thickness, Color color) {
    const std::array corners{Vector2{center.x, center.y - halfHeight}, Vector2{center.x + halfWidth, center.y},
                             Vector2{center.x, center.y + halfHeight}, Vector2{center.x - halfWidth, center.y}};
    for (std::size_t i = 0; i < corners.size(); ++i)
        DrawLineEx(corners[i], corners[(i + 1) % corners.size()], thickness, color);
}

Vector2 RowSlot(Vector2 center, int count, int index, float gap) {
    return {center.x + gap * (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f), center.y};
}

void DrawLightning(Vector2 from, Vector2 to, int seed, Color color, float alpha, const LightningStyle& style) {
    if (style.segments <= 0) return;
    Vector2 previous = from;
    for (int i = 1; i <= style.segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(style.segments);
        const float wobble = i == style.segments ? 0.0f : std::sin(static_cast<float>(i * 17 + seed * 31) * 12.9898f);
        const Vector2 point{from.x + (to.x - from.x) * t + wobble * style.jitter * std::sin(t * PI),
                            from.y + (to.y - from.y) * t};
        DrawLineEx(previous, point, style.width, Fade(color, alpha));
        if (style.coreWidth > 0.0f) DrawLineEx(previous, point, style.coreWidth, Fade(style.core, alpha));
        previous = point;
    }
    if (style.glowRadius > 0.0f) DrawGlow(to, style.glowRadius, Fade(color, style.glowAlpha * alpha));
}
