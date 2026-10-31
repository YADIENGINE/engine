#pragma once
#include <string_view>
#include "Graphics/Anim/ButtonAnim.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;
struct TextStyle;

// Neon widgets with explicit styles (the theme's are in CurrentTheme().ui; Graphics/Ui/UiDraw.h draws with them).
// Glow follows Painter::Quality(): bands and halos weaken or disappear with the player's glow setting.

// A glowing line: two translucent bands added around it (outer only at full glow quality), the line itself and an
// optional whitened core. Widths multiply the line's thickness.
struct NeonFrameStyle {
    float outerWidth = 1.0f;
    float innerWidth = 1.0f;
    float outerAlpha = 0.0f;
    float innerAlpha = 0.0f;
    float coreWidth = 0.0f;
    float coreMinWidth = 0.0f;
    float coreWhiten = 0.0f;
};
// Glowing text: a halo of 'haloRadius' at 'haloAlpha' times the text's alpha, and the text whitened towards its
// centre by 'coreWhiten'. Both scale with the call's 'glow'.
struct NeonTextStyle {
    float haloRadius = 0.0f;
    float haloAlpha = 0.0f;
    float coreWhiten = 0.0f;
};
struct NeonPanelStyle {
    Color4 body{0, 0, 0, 0};
    Color4 shine{0, 0, 0, 0};
    float notch = 0.0f;
    float frameWidth = 0.0f;
    float glow = 0.0f;
    float shineOffset = 0.0f;
    float shineHeight = 0.0f;
    NeonFrameStyle frame;
};
struct NeonChipStyle {
    Color4 text{255, 255, 255, 255};
    float paddingX = 0.0f;
    float paddingY = 0.0f;
    float notch = 0.0f;
    float glowRadiusScale = 0.0f;
    float glowAlpha = 0.0f;
};
struct ArcadeButtonStyle {
    Color4 body{0, 0, 0, 0};
    Color4 shadow{0, 0, 0, 0};
    Color4 frameColor{255, 255, 255, 255};
    Color4 label{255, 255, 255, 255};
    Color4 shine{0, 0, 0, 0};
    Color4 pressColor{255, 255, 255, 255};
    float depth = 0.0f;
    float notch = 0.0f;
    float grow = 0.0f;
    float squash = 0.0f;
    float frameWidth = 0.0f;
    float appearDrop = 0.0f;
    float appearAlphaScale = 1.0f;
    float shakeAmount = 0.0f;
    float shakeSpeed = 0.0f;
    float pulseSpeed = 0.0f;
    float pulseAmount = 0.0f;
    float pressFlash = 0.0f;
    float darkAccent = 0.0f;
    float shadowAccent = 0.0f;
    float labelWhiten = 0.0f;
    float shineOffset = 0.0f;
    float shineHeight = 0.0f;
    float shineHighlight = 0.0f;
    float padding = 0.0f;
    NeonFrameStyle frame;
    NeonTextStyle text;
};
struct RoundButtonStyle {
    Color4 body{0, 0, 0, 0};
    Color4 shadow{0, 0, 0, 0};
    float shadowOffset = 0.0f;
    float frameWidth = 0.0f;
    float glowInnerWidth = 0.0f;
    float glowOuterWidth = 0.0f;
    float glowAlpha = 0.0f;
    float labelWhiten = 0.0f;
    float textGlow = 0.0f;
    float padding = 0.0f;
    NeonTextStyle text;
};
// Key symbol (Q / E): rounded box with the letter in the middle.
struct KeyCapStyle {
    Color4 body{0, 0, 0, 0};
    Color4 frame{0, 0, 0, 0};
    Color4 label{255, 255, 255, 255};
    float roundness = 0.0f; // Corner radius as a share of half the shorter side
    float frameWidth = 0.0f;
};
// Arcade progress bar: dark track, leading ghost band, glowing fill, top shine, block gaps, neon frame. A zero width
// or alpha turns that layer off.
struct ProgressBarStyle {
    Color4 track{0, 0, 0, 255};
    Color4 ghost{0, 0, 0, 0};  // Progress that arrived but the fill has not caught up with yet
    float previewAlpha = 0.0f; // The preview slice is drawn in the fill colour at this alpha
    Color4 shine{0, 0, 0, 0};  // Top band of the fill
    float shineHeight = 0.0f;  // Share of the height
    float glowAlpha = 0.0f;    // Additive glow of the fill
    float glowSpread = 0.0f;
    float segment = 0.0f; // Block width
    float segmentGap = 0.0f;
    Color4 flash{0, 0, 0, 0}; // Covers the bar while ProgressBarLook::flash is 1
    Color4 frame{0, 0, 0, 0};
    float frameOutset = 0.0f;
    float frameWidth = 0.0f;
    float frameGlow = 0.0f;
    NeonFrameStyle frameStyle;
};
struct ProgressBarLook {
    float fill = 0.0f;  // 0..1
    float ghost = 0.0f; // 0..1; the part beyond 'fill' shows as the leading band
    // 0..1; the part beyond 'fill' and 'ghost' shows as a faint preview (where a gain not yet arrived will lead)
    float preview = 0.0f;
    Color4 color{255, 255, 255, 255};
    float flash = 0.0f; // 0..1
    // 0..1; a second fill drawn from the left over the fill in its own colour (time running out over a full bar)
    float overlay = 0.0f;
    Color4 overlayColor{0, 0, 0, 0};
    float alpha = 1.0f;
};
// A button's look this frame: its animation (UiButton), entrance (0..1), offset and accent colour.
struct ButtonLook {
    ButtonAnimState animation;
    float appear = 1.0f;
    Float2 offset{};
    Color4 accent{255, 255, 255, 255};
};

void DrawNeonFrame(Painter& paint, const FloatRect& rect, float notch, float thickness, Color4 color, float glow,
                   const NeonFrameStyle& style);
// 'text' gives size, colour, alignment and fitting; the neon style adds the halo and the whitened core.
void DrawNeonText(Painter& paint, std::string_view label, Float2 position, const TextStyle& text, float glow,
                  const NeonTextStyle& style);
void DrawNeonPanel(Painter& paint, const FloatRect& rect, Color4 accent, const NeonPanelStyle& style);
// Filled label tag around 'center'.
void DrawNeonChip(Painter& paint, std::string_view label, Float2 center, float size, Color4 color,
                  const NeonChipStyle& style);
// The button without its label: returns the animated body rectangle for custom content. 'time' drives the deny shake
// and the highlight pulse.
FloatRect DrawArcadeButtonBody(Painter& paint, const FloatRect& rect, const ButtonLook& look, float time,
                               const ArcadeButtonStyle& style);
void DrawArcadeButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, const ButtonLook& look,
                      float time, const ArcadeButtonStyle& style);
void DrawRoundButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, Color4 accent,
                     const RoundButtonStyle& style);
void DrawKeyCap(Painter& paint, const FloatRect& area, std::string_view key, float size, const KeyCapStyle& style);
// A 'width' thick ring inside 'radius' with an additive halo of 'glow' alpha around it.
void DrawNeonRing(Painter& paint, Float2 center, float radius, float width, Color4 color, float glow);
void DrawProgressBar(Painter& paint, const FloatRect& bar, const ProgressBarLook& look, const ProgressBarStyle& style);
