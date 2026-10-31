#include "Graphics/Draw/Neon.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr Color4 White{255, 255, 255, 255};

float ClampNotch(const FloatRect& rect, float notch) {
    return std::max(0.0f, std::min({notch, rect.width * 0.5f, rect.height * 0.5f}));
}

// 'color' moved towards white by 't', keeping its alpha.
Color4 Whiten(Color4 color, float t) { return Mix(color, Color4{255, 255, 255, color.a}, t); }

bool Glows(const Painter& paint, float glow) { return glow > 0.0f && paint.Quality().glow != GlowQuality::Off; }
} // namespace

void DrawNeonFrame(Painter& paint, const FloatRect& rect, float notch, float thickness, Color4 color, float glow,
                   const NeonFrameStyle& style) {
    if (rect.Empty() || !(thickness > 0.0f)) return;
    const float chamfer = ClampNotch(rect, notch);
    if (Glows(paint, glow)) {
        auto light = paint.Blending(Blend::Add);
        if (style.outerWidth > 0.0f && paint.Quality().glow == GlowQuality::Full)
            paint.ChamferRect(rect, chamfer,
                              Outline(thickness * style.outerWidth, ScaleAlpha(color, style.outerAlpha * glow)));
        if (style.innerWidth > 0.0f)
            paint.ChamferRect(rect, chamfer,
                              Outline(thickness * style.innerWidth, ScaleAlpha(color, style.innerAlpha * glow)));
    }
    paint.ChamferRect(rect, chamfer, Outline(thickness, color));
    if (style.coreWidth > 0.0f || style.coreMinWidth > 0.0f)
        paint.ChamferRect(
            rect, chamfer,
            Outline(std::max(style.coreMinWidth, thickness * style.coreWidth), Whiten(color, style.coreWhiten * glow)));
}

void DrawNeonText(Painter& paint, std::string_view label, Float2 position, const TextStyle& text, float glow,
                  const NeonTextStyle& style) {
    TextStyle look = text;
    if (glow > 0.0f && style.haloRadius > 0.0f && style.haloAlpha > 0.0f)
        look.glow = {style.haloRadius, text.color, style.haloAlpha * glow};
    look.color = Whiten(text.color, style.coreWhiten * std::max(0.0f, glow));
    paint.Text(label, position, look);
}

void DrawNeonPanel(Painter& paint, const FloatRect& rect, Color4 accent, const NeonPanelStyle& style) {
    if (rect.Empty()) return;
    const float chamfer = ClampNotch(rect, style.notch);
    paint.ChamferRect(rect, chamfer, Fill(style.body));
    if (style.shineHeight > 0.0f)
        paint.Rect({rect.x + chamfer, rect.y + style.shineOffset, rect.width - 2.0f * chamfer, style.shineHeight},
                   Fill(style.shine));
    DrawNeonFrame(paint, rect, style.notch, style.frameWidth, accent, style.glow, style.frame);
}

void DrawNeonChip(Painter& paint, std::string_view label, Float2 center, float size, Color4 color,
                  const NeonChipStyle& style) {
    const float width = paint.Font().Measure(label, size).x + 2.0f * style.paddingX;
    const FloatRect body = FloatRect::FromCenter(center, {width, size + 2.0f * style.paddingY});
    if (style.glowRadiusScale > 0.0f && style.glowAlpha > 0.0f)
        paint.Glow(center, width * style.glowRadiusScale, {.color = ScaleAlpha(color, style.glowAlpha)});
    paint.ChamferRect(body, ClampNotch(body, style.notch), Fill(color));
    paint.Text(label, {center.x, body.y + style.paddingY},
               {.size = size,
                .color = ScaleAlpha(style.text, static_cast<float>(color.a) / 255.0f),
                .align = TextAlign::Center});
}

FloatRect DrawArcadeButtonBody(Painter& paint, const FloatRect& rect, const ButtonLook& look, float time,
                               const ArcadeButtonStyle& style) {
    if (look.appear <= 0.0f || rect.Empty()) return {};
    const float alpha = Clamp01(look.appear * style.appearAlphaScale);
    const float highlight = std::max(0.0f, look.animation.highlight);
    const float press = Clamp01(look.animation.press);
    const float scale = 1.0f + style.grow * look.animation.highlight - style.squash * press;
    const Float2 size = rect.Size() * scale;
    const Float2 center = rect.Center() + look.offset +
                          Float2{look.animation.shake * style.shakeAmount * std::sin(time * style.shakeSpeed),
                                 (1.0f - EaseOutBack(look.appear)) * style.appearDrop};

    const float top = center.y - size.y * 0.5f - style.depth * 0.5f;
    const FloatRect shadow{center.x - size.x * 0.5f, top + style.depth, size.x, size.y};
    const FloatRect body{shadow.x, top + style.depth * press, size.x, size.y};
    const float chamfer = ClampNotch(body, style.notch);

    const float pulse = 1.0f - style.pulseAmount + style.pulseAmount * std::sin(time * style.pulseSpeed);
    const Color4 darkAccent = Mix(style.body, look.accent, style.darkAccent);
    paint.ChamferRect(shadow, chamfer,
                      Fill(ScaleAlpha(Mix(style.shadow, darkAccent, style.shadowAccent * highlight), alpha)));
    paint.ChamferRect(body, chamfer, Fill(ScaleAlpha(Mix(style.body, darkAccent, Clamp01(highlight)), alpha)));
    if (style.shineHeight > 0.0f)
        paint.Rect({body.x + chamfer, body.y + style.shineOffset, body.width - 2.0f * chamfer, style.shineHeight},
                   Fill(ScaleAlpha(style.shine, alpha * (1.0f + style.shineHighlight * highlight))));
    if (press > 0.0f)
        paint.ChamferRect(body, chamfer, Fill(ScaleAlpha(style.pressColor, style.pressFlash * press * alpha)));
    const Color4 frame = Mix(style.frameColor, look.accent, Clamp01(highlight + press));
    DrawNeonFrame(paint, body, style.notch, style.frameWidth, ScaleAlpha(frame, alpha),
                  Clamp01(highlight * pulse + press), style.frame);
    return body;
}

void DrawArcadeButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, const ButtonLook& look,
                      float time, const ArcadeButtonStyle& style) {
    const FloatRect body = DrawArcadeButtonBody(paint, rect, look, time, style);
    if (body.Empty()) return;
    const float labelSize = size * body.width / rect.width;
    const float alpha = Clamp01(look.appear * style.appearAlphaScale);
    const float highlight = Clamp01(look.animation.highlight);
    const Color4 color = Mix(style.label, Whiten(look.accent, style.labelWhiten), highlight);
    DrawNeonText(paint, label, {body.Center().x, body.Center().y - labelSize * 0.5f},
                 {.size = labelSize,
                  .color = ScaleAlpha(color, alpha),
                  .align = TextAlign::Center,
                  .maxWidth = std::max(0.0f, body.width - style.padding),
                  .fit = TextFit::Shrink},
                 highlight, style.text);
}

void DrawRoundButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, Color4 accent,
                     const RoundButtonStyle& style) {
    if (rect.Empty()) return;
    const Float2 center = rect.Center();
    const float radius = std::min(rect.width, rect.height) * 0.5f;
    paint.Circle(center + Float2{0.0f, style.shadowOffset}, radius, Fill(style.shadow));
    paint.Circle(center, radius, Fill(style.body));
    if (Glows(paint, style.glowAlpha) && (style.glowInnerWidth > 0.0f || style.glowOuterWidth > 0.0f)) {
        const float inner = std::max(0.0f, radius - style.glowInnerWidth);
        const float outer = radius + style.glowOuterWidth;
        auto light = paint.Blending(Blend::Add);
        paint.Circle(center, (inner + outer) * 0.5f, Outline(outer - inner, ScaleAlpha(accent, style.glowAlpha)));
    }
    if (style.frameWidth > 0.0f) paint.Circle(center, radius, Outline(style.frameWidth, accent, StrokeAlign::Inside));
    DrawNeonText(paint, label, {center.x, center.y - size * 0.5f},
                 {.size = size,
                  .color = Whiten(accent, style.labelWhiten),
                  .align = TextAlign::Center,
                  .maxWidth = std::max(0.0f, rect.width - style.padding),
                  .fit = TextFit::Shrink},
                 style.textGlow, style.text);
}

void DrawKeyCap(Painter& paint, const FloatRect& area, std::string_view key, float size, const KeyCapStyle& style) {
    if (area.Empty()) return;
    const float radius = style.roundness * std::min(area.width, area.height) * 0.5f;
    paint.RoundRect(area, radius, {.fill = style.body, .stroke = {style.frameWidth, style.frame, StrokeAlign::Inside}});
    paint.Text(key, {area.Center().x, area.Center().y - size * 0.5f},
               {.size = size, .color = style.label, .align = TextAlign::Center});
}

void DrawNeonRing(Painter& paint, Float2 center, float radius, float width, Color4 color, float glow) {
    if (!(width > 0.0f)) return;
    if (Glows(paint, glow)) {
        auto light = paint.Blending(Blend::Add);
        paint.Circle(center, radius - 0.5f * width, Outline(3.0f * width, WithAlpha(color, glow)));
    }
    paint.Circle(center, radius, Outline(width, color, StrokeAlign::Inside));
}

void DrawProgressBar(Painter& paint, const FloatRect& bar, const ProgressBarLook& look, const ProgressBarStyle& style) {
    if (bar.Empty()) return;
    const float alpha = look.alpha;
    const auto slice = [&](float from, float to) {
        return FloatRect{bar.x + bar.width * from, bar.y, bar.width * (to - from), bar.height};
    };
    const float fill = Clamp01(look.fill);
    paint.Rect(bar, Fill(ScaleAlpha(style.track, alpha)));
    if (look.ghost > 0.0f) paint.Rect(slice(0.0f, Clamp01(look.ghost)), Fill(ScaleAlpha(style.ghost, alpha)));
    const float previewStart = std::max(fill, Clamp01(look.ghost));
    if (Clamp01(look.preview) > previewStart && style.previewAlpha > 0.0f)
        paint.Rect(slice(previewStart, Clamp01(look.preview)),
                   Fill(ScaleAlpha(look.color, style.previewAlpha * alpha)));
    if (fill > 0.0f && Glows(paint, style.glowAlpha)) {
        auto light = paint.Blending(Blend::Add);
        paint.Rect(slice(0.0f, fill).Inset(0.0f, -style.glowSpread),
                   Fill(ScaleAlpha(look.color, style.glowAlpha * alpha)));
    }
    if (fill > 0.0f) paint.Rect(slice(0.0f, fill), Fill(ScaleAlpha(look.color, alpha)));
    if (look.overlay > 0.0f) paint.Rect(slice(0.0f, Clamp01(look.overlay)), Fill(ScaleAlpha(look.overlayColor, alpha)));
    if (fill > 0.0f && style.shineHeight > 0.0f)
        paint.Rect({bar.x, bar.y, bar.width * fill, bar.height * style.shineHeight},
                   Fill(ScaleAlpha(style.shine, alpha)));
    if (style.segment > 0.0f)
        for (float x = bar.x + style.segment; x < bar.Right(); x += style.segment)
            paint.Rect({x - style.segmentGap * 0.5f, bar.y, style.segmentGap, bar.height},
                       Fill(ScaleAlpha(style.track, alpha)));
    if (look.flash > 0.0f) paint.Rect(bar, Fill(ScaleAlpha(style.flash, look.flash * alpha)));
    if (style.frameWidth > 0.0f)
        DrawNeonFrame(paint, bar.Inset(-style.frameOutset), style.frameOutset, style.frameWidth,
                      ScaleAlpha(style.frame, alpha), style.frameGlow, style.frameStyle);
}
