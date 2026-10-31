#pragma once
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

// Scanline geometry; the darkness is given when drawing.
struct ScanlineStyle {
    Color4 color{0, 0, 0, 255}; // Opaque colour, faded by the darkness given when drawing
    float spacing = 4.0f;
    float thickness = 1.0f;
};

// Screen background: scrolling grid, edge darkening and scanline darkness. The layers are drawn separately; the
// caller chooses their order.
struct BackdropStyle {
    float gridCell = 50.0f;
    float gridScrollSpeed = 0.0f; // Units per second, downwards
    Color4 gridColor{0, 0, 0, 0};
    float vignetteSize = 0.0f; // 0: no vignette
    Color4 vignetteColor{0, 0, 0, 0};
    float scanlineAlpha = 0.0f;
};

// Lines every 'spacing' units across 'bounds', shifted by 'offset'; nonpositive spacing is invalid_argument,
// nonpositive thickness draws nothing.
void DrawGrid2D(Painter& paint, const FloatRect& bounds, float spacing, Float2 offset, Color4 color, float thickness);
// Skipped when background effects are off (Painter::Quality).
void DrawScanlines(Painter& paint, const FloatRect& bounds, Color4 color, float spacing, float thickness);
void DrawScanlines(Painter& paint, const FloatRect& bounds, const ScanlineStyle& style, float alpha);
// Edges darkening inwards over 'size' units; nonpositive size draws nothing.
void DrawVignette(Painter& paint, const FloatRect& bounds, float size, Color4 color);

// The grid scrolls with Painter::Time() (stands still when background effects are off).
void DrawBackdropGrid(Painter& paint, const BackdropStyle& style, const FloatRect& area);
void DrawBackdropVignette(Painter& paint, const BackdropStyle& style, const FloatRect& area);
void DrawBackdropScanlines(Painter& paint, const BackdropStyle& style, const ScanlineStyle& scanlines,
                           const FloatRect& area);
