#include "Graphics/Draw/Neon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/EffectQuality.h"
#include "Graphics/Draw/TextRenderer.h"

namespace {
float ClampNotch(Rectangle rect, float notch) {
    return std::max(0.0f, std::min({notch, rect.width / 2.0f, rect.height / 2.0f}));
}

std::array<Vector2, 8> NotchedCorners(Rectangle r, float n) {
    return {{{r.x + n, r.y},
             {r.x + r.width - n, r.y},
             {r.x + r.width, r.y + n},
             {r.x + r.width, r.y + r.height - n},
             {r.x + r.width - n, r.y + r.height},
             {r.x + n, r.y + r.height},
             {r.x, r.y + r.height - n},
             {r.x, r.y + n}}};
}

void DrawNotchedOutline(Rectangle rect, float notch, float thickness, Color color) {
    const auto corners = NotchedCorners(rect, ClampNotch(rect, notch));
    for (std::size_t i = 0; i < corners.size(); ++i) {
        DrawLineEx(corners[i], corners[(i + 1) % corners.size()], thickness, color);
        DrawCircleV(corners[i], thickness / 2.0f, color); // Köşe birleşimi boşluk bırakmasın
    }
}

} // namespace

void FillNotched(Rectangle r, float notch, Color color) {
    if (r.width <= 0.0f || r.height <= 0.0f) return;
    const float n = ClampNotch(r, notch);
    DrawRectangleRec({r.x + n, r.y, r.width - 2.0f * n, r.height}, color);
    DrawRectangleRec({r.x, r.y + n, n, r.height - 2.0f * n}, color);
    DrawRectangleRec({r.x + r.width - n, r.y + n, n, r.height - 2.0f * n}, color);
    const float right = r.x + r.width;
    const float bottom = r.y + r.height;
    FillTriangle({r.x + n, r.y}, {r.x, r.y + n}, {r.x + n, r.y + n}, color);
    FillTriangle({right - n, r.y}, {right - n, r.y + n}, {right, r.y + n}, color);
    FillTriangle({r.x, bottom - n}, {r.x + n, bottom}, {r.x + n, bottom - n}, color);
    FillTriangle({right - n, bottom - n}, {right - n, bottom}, {right, bottom - n}, color);
}

void DrawNeonFrame(Rectangle rect, float notch, float thickness, Color color, float glow, const NeonFrameStyle& style) {
    if (rect.width <= 0.0f || rect.height <= 0.0f || thickness <= 0.0f) return;
    const EffectLevel level = CurrentEffectQuality().glow;
    if (glow > 0.0f && level != EffectLevel::Off) {
        BeginBlendMode(BLEND_ADDITIVE);
        if (style.outerWidth > 0.0f && level == EffectLevel::Full)
            DrawNotchedOutline(rect, notch, thickness * style.outerWidth, Alpha(color, style.outerAlpha * glow));
        if (style.innerWidth > 0.0f)
            DrawNotchedOutline(rect, notch, thickness * style.innerWidth, Alpha(color, style.innerAlpha * glow));
        EndBlendMode();
    }
    DrawNotchedOutline(rect, notch, thickness, color);
    if (style.coreWidth > 0.0f || style.coreMinWidth > 0.0f)
        DrawNotchedOutline(
            rect, notch, std::max(style.coreMinWidth, thickness * style.coreWidth),
            ColorLerp(color, Alpha(WHITE, static_cast<float>(color.a) / 255.0f), style.coreWhiten * glow));
}

void DrawNeonText(const TextRenderer& text, const char* label, int centerX, int y, int fontSize, Color color,
                  int maxWidth, float glow, const NeonTextStyle& style) {
    const EffectLevel level = CurrentEffectQuality().glow;
    if (glow > 0.0f && level != EffectLevel::Off && (style.glowAlpha > 0.0f || style.samples > 0)) {
        BeginBlendMode(BLEND_ADDITIVE);
        if (style.glowAlpha > 0.0f) {
            const int offset = style.glowOffset;
            const Color halo = Alpha(color, style.glowAlpha * glow);
            text.DrawCentered(label, centerX - offset, y, fontSize, halo, maxWidth);
            text.DrawCentered(label, centerX + offset, y, fontSize, halo, maxWidth);
            text.DrawCentered(label, centerX, y - offset, fontSize, halo, maxWidth);
            text.DrawCentered(label, centerX, y + offset, fontSize, halo, maxWidth);
        }
        for (const NeonTextRing& ring : style.rings) {
            if (level != EffectLevel::Full || style.samples <= 0 || ring.alpha <= 0.0f) continue;
            const Color halo = Alpha(color, ring.alpha * glow);
            for (int i = 0; i < style.samples; ++i) {
                const float angle =
                    (static_cast<float>(i) + ring.phase) * 2.0f * PI / static_cast<float>(style.samples);
                const int dx = static_cast<int>(std::lround(std::cos(angle) * ring.radius));
                const int dy = static_cast<int>(std::lround(std::sin(angle) * ring.radius));
                text.DrawCentered(label, centerX + dx, y + dy, fontSize, halo, maxWidth);
            }
        }
        EndBlendMode();
    }
    const Color core = ColorLerp(color, Alpha(WHITE, static_cast<float>(color.a) / 255.0f), style.coreWhiten * glow);
    text.DrawCentered(label, centerX, y, fontSize, core, maxWidth);
}

void DrawNeonPanel(const Rect& rect, Color accent, const NeonPanelStyle& style) {
    if (rect.width <= 0 || rect.height <= 0) return;
    const Rectangle r = ToRectangle(rect);
    FillNotched(r, style.notch, style.body);
    DrawRectangleRec({r.x + style.notch, r.y + style.shineOffset, r.width - 2.0f * style.notch, style.shineHeight},
                     style.shine);
    DrawNeonFrame(r, style.notch, style.frameWidth, accent, style.glow, style.frame);
}

void DrawNeonChip(const TextRenderer& text, const char* label, Vector2 center, int fontSize, Color color,
                  const NeonChipStyle& style) {
    const float width = static_cast<float>(text.Measure(label, fontSize) + 2 * style.paddingX);
    const float height = static_cast<float>(fontSize + 2 * style.paddingY);
    const Rectangle r{center.x - width / 2.0f, center.y - height / 2.0f, width, height};
    if (style.glowRadiusScale > 0.0f && style.glowAlpha > 0.0f) {
        DrawGlow(center, width * style.glowRadiusScale, Alpha(color, style.glowAlpha));
    }
    FillNotched(r, style.notch, color);
    text.DrawCentered(label, static_cast<int>(center.x), static_cast<int>(r.y) + style.paddingY, fontSize,
                      Alpha(style.text, static_cast<float>(color.a) / 255.0f));
}

Rectangle DrawArcadeButtonBody(const Rect& rect, const ButtonLook& look, float timeSeconds,
                               const ArcadeButtonStyle& style) {
    if (look.appear <= 0.0f || rect.width <= 0 || rect.height <= 0) {
        return {};
    }
    const float alpha = Clamp01(look.appear * style.appearAlphaScale);
    const float highlight = std::max(0.0f, look.animation.highlight);
    const float press = Clamp01(look.animation.press);
    const float scale = 1.0f + style.grow * look.animation.highlight - style.squash * press;
    const float width = static_cast<float>(rect.width) * scale;
    const float height = static_cast<float>(rect.height) * scale;
    const float centerX = static_cast<float>(rect.CenterX()) + look.offset.x +
                          look.animation.shake * style.shakeAmount * std::sin(timeSeconds * style.shakeSpeed);
    const float centerY =
        static_cast<float>(rect.CenterY()) + look.offset.y + (1.0f - EaseOutBack(look.appear)) * style.appearDrop;

    const float top = centerY - height / 2.0f - style.depth / 2.0f;
    const Rectangle shadow{centerX - width / 2.0f, top + style.depth, width, height};
    const Rectangle body{shadow.x, top + style.depth * press, width, height};

    const float pulse = 1.0f - style.pulseAmount + style.pulseAmount * std::sin(timeSeconds * style.pulseSpeed);
    const Color accent = look.accent;
    const Color darkAccent = ColorLerp(style.body, accent, style.darkAccent);

    FillNotched(shadow, style.notch, Alpha(ColorLerp(style.shadow, darkAccent, style.shadowAccent * highlight), alpha));
    FillNotched(body, style.notch, Alpha(ColorLerp(style.body, darkAccent, Clamp01(highlight)), alpha));
    DrawRectangleRec(
        {body.x + style.notch, body.y + style.shineOffset, body.width - 2.0f * style.notch, style.shineHeight},
        Alpha(style.shine, alpha * (1.0f + style.shineHighlight * highlight)));
    if (press > 0.0f) {
        FillNotched(body, style.notch, Alpha(style.pressColor, style.pressFlash * press * alpha));
    }
    const Color frame = ColorLerp(style.frameColor, accent, Clamp01(highlight + press));
    DrawNeonFrame(body, style.notch, style.frameWidth, Alpha(frame, alpha), Clamp01(highlight * pulse + press),
                  style.frame);
    return body;
}

void DrawArcadeButton(const Rect& rect, const char* label, int fontSize, const ButtonLook& look,
                      const TextRenderer& text, float timeSeconds, const ArcadeButtonStyle& style) {
    if (look.appear <= 0.0f || rect.width <= 0 || rect.height <= 0) {
        return;
    }
    const Rectangle body = DrawArcadeButtonBody(rect, look, timeSeconds, style);
    const float scale = body.width / static_cast<float>(rect.width);
    const float alpha = Clamp01(look.appear * style.appearAlphaScale);
    const float highlight = std::max(0.0f, look.animation.highlight);
    const Color accent = look.accent;
    const float centerX = body.x + body.width / 2.0f;
    const int labelSize = std::max(1, static_cast<int>(std::lround(static_cast<float>(fontSize) * scale)));
    const Color labelColor = ColorLerp(style.label, ColorLerp(accent, WHITE, style.labelWhiten), Clamp01(highlight));
    DrawNeonText(text, label, static_cast<int>(centerX), static_cast<int>(body.y + body.height / 2.0f) - labelSize / 2,
                 labelSize, Alpha(labelColor, alpha), static_cast<int>(body.width) - style.padding, Clamp01(highlight),
                 style.text);
}

void DrawRoundButton(const Rect& rect, const char* label, int fontSize, Color accent, const TextRenderer& text,
                     const RoundButtonStyle& style) {
    if (rect.width <= 0 || rect.height <= 0) return;
    const Vector2 center{static_cast<float>(rect.CenterX()), static_cast<float>(rect.CenterY())};
    const float radius = static_cast<float>(std::min(rect.width, rect.height)) / 2.0f;
    DrawCircleV({center.x, center.y + style.shadowOffset}, radius, style.shadow);
    DrawCircleV(center, radius, style.body);
    if (CurrentEffectQuality().glow != EffectLevel::Off && style.glowAlpha > 0.0f &&
        (style.glowInnerWidth > 0.0f || style.glowOuterWidth > 0.0f)) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRing(center, std::max(0.0f, radius - style.glowInnerWidth), radius + style.glowOuterWidth, 0.0f, 360.0f,
                 style.segments, Alpha(accent, style.glowAlpha));
        EndBlendMode();
    }
    if (style.frameWidth > 0.0f)
        DrawRing(center, std::max(0.0f, radius - style.frameWidth), radius, 0.0f, 360.0f, style.segments, accent);
    DrawNeonText(text, label, rect.CenterX(), rect.CenterY() - fontSize / 2, fontSize,
                 ColorLerp(accent, WHITE, style.labelWhiten), rect.width - style.padding, style.textGlow, style.text);
}

void DrawNeonRing(Vector2 center, float radius, float width, Color color, float glow, int segments) {
    if (glow > 0.0f && CurrentEffectQuality().glow != EffectLevel::Off) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRing(center, radius - 2.0f * width, radius + width, 0.0f, 360.0f, segments, Fade(color, glow));
        EndBlendMode();
    }
    DrawRing(center, radius - width, radius, 0.0f, 360.0f, segments, color);
}

void DrawProgressBar(Rectangle bar, const ProgressBarLook& look, const ProgressBarStyle& style) {
    if (bar.width <= 0.0f || bar.height <= 0.0f) return;
    const float alpha = look.alpha;
    const float fillWidth = bar.width * Clamp01(look.fill);
    DrawRectangleRec(bar, Alpha(style.track, alpha));
    if (look.ghost > 0.0f)
        DrawRectangleRec({bar.x, bar.y, bar.width * Clamp01(look.ghost), bar.height}, Alpha(style.ghost, alpha));
    const float previewStart = std::max(Clamp01(look.fill), Clamp01(look.ghost));
    if (Clamp01(look.preview) > previewStart && style.previewAlpha > 0.0f)
        DrawRectangleRec(
            {bar.x + bar.width * previewStart, bar.y, bar.width * (Clamp01(look.preview) - previewStart), bar.height},
            Alpha(look.color, style.previewAlpha * alpha));
    if (fillWidth > 0.0f && style.glowAlpha > 0.0f && CurrentEffectQuality().glow != EffectLevel::Off) {
        BeginBlendMode(BLEND_ADDITIVE);
        DrawRectangleRec({bar.x, bar.y - style.glowSpread, fillWidth, bar.height + 2.0f * style.glowSpread},
                         Alpha(look.color, style.glowAlpha * alpha));
        EndBlendMode();
    }
    DrawRectangleRec({bar.x, bar.y, fillWidth, bar.height}, Alpha(look.color, alpha));
    if (look.overlay > 0.0f)
        DrawRectangleRec({bar.x, bar.y, bar.width * Clamp01(look.overlay), bar.height},
                         Alpha(look.overlayColor, alpha));
    if (style.shineHeight > 0.0f)
        DrawRectangleRec({bar.x, bar.y, fillWidth, bar.height * style.shineHeight}, Alpha(style.shine, alpha));
    if (style.segment > 0.0f) {
        for (float x = bar.x + style.segment; x < bar.x + bar.width; x += style.segment)
            DrawRectangleRec({x - style.segmentGap / 2.0f, bar.y, style.segmentGap, bar.height},
                             Alpha(style.track, alpha));
    }
    if (look.flash > 0.0f) DrawRectangleRec(bar, Alpha(style.flash, look.flash * alpha));
    if (style.frameWidth > 0.0f) {
        const float outset = style.frameOutset;
        DrawNeonFrame({bar.x - outset, bar.y - outset, bar.width + 2.0f * outset, bar.height + 2.0f * outset}, outset,
                      style.frameWidth, Alpha(style.frame, alpha), style.frameGlow, style.frameStyle);
    }
}
