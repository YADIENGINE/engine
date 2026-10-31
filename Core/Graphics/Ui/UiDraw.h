#pragma once
#include <string_view>
#include "Graphics/Draw/Neon.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;
struct TextStyle;

// Themed widgets: styles come from the active theme (CurrentTheme().ui), animation time from Painter::Time(), text
// from the game's font. For a one-off variant copy a style and call the explicit-style function
// (Graphics/Draw/Neon.h: DrawNeon*, DrawArcadeButton ...); drawing with its own clock does the same.
void DrawUiFrame(Painter& paint, const FloatRect& rect, float notch, float thickness, Color4 color, float glow = 1.0f);
// 'text' gives size, colour, alignment and fitting; the theme adds the neon halo.
void DrawUiText(Painter& paint, std::string_view label, Float2 position, const TextStyle& text, float glow = 1.0f);
void DrawUiPanel(Painter& paint, const FloatRect& rect, Color4 accent);
void DrawUiChip(Painter& paint, std::string_view label, Float2 center, float size, Color4 color);
// The button's body without a label; returns the animated body rectangle for a custom label or icon.
FloatRect DrawUiButtonBody(Painter& paint, const FloatRect& rect, const ButtonLook& look);
void DrawUiButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, const ButtonLook& look);
void DrawUiRoundButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, Color4 accent);
// Key symbol (Q / E).
void DrawUiKeyCap(Painter& paint, const FloatRect& area, std::string_view key, float size);
void DrawUiProgressBar(Painter& paint, const FloatRect& bar, const ProgressBarLook& look);
