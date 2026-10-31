#pragma once
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

// Small shapes views share, drawn with Painter (canvas coordinates). Plain shapes are Painter calls themselves
// (paint.Triangle, paint.Polygon, paint.Glow ...).

// Zigzag lightning: breaks down from 'from' to 'to'; 'seed' changes its shape. A glow of 'glowRadius' at the end.
struct LightningStyle {
    int segments = 8;
    float jitter = 0.0f; // Largest sideways offset (in the middle)
    float width = 1.0f;  // Outer line, in the lightning's colour
    float coreWidth = 0.0f;
    Color4 core{255, 255, 255, 255}; // Inner line
    float glowRadius = 0.0f;
    float glowAlpha = 0.0f;
};

// Centre of item 'index' in a row of 'count' items 'gap' apart, centred on 'center' (pips, dots, badges).
[[nodiscard]] constexpr Float2 RowSlot(Float2 center, int count, int index, float gap) {
    return {center.x + gap * (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f), center.y};
}

// Five-pointed star around 'center'.
void DrawStar(Painter& paint, Float2 center, float outerRadius, const ShapeStyle& style);
// Rhombus with its corners above, below and beside 'center'; 'half' is half its width and height.
void DrawDiamond(Painter& paint, Float2 center, Float2 half, const ShapeStyle& style);
// Thick arc between 'inner' and 'outer' radii from 'start' over 'sweep' radians (clockwise when positive);
// a full turn draws a ring. Ignores style.width; uses outer - inner instead.
void DrawBand(Painter& paint, Float2 center, float inner, float outer, float start, float sweep,
              const LineStyle& style);
// Ring between 'inner' and 'outer' radii; the stroke width is outer - inner.
void DrawRing(Painter& paint, Float2 center, float inner, float outer, const ShapeStyle& style);
// Regular polygon with 'sides' corners on a circle of 'radius'; the first corner points 'radians' clockwise from +x.
void DrawRegularPolygon(Painter& paint, Float2 center, int sides, float radius, float radians, const ShapeStyle& style);
// Rectangle turned 'radians' (clockwise on screen) around its centre; 'half' is half its width and height.
void DrawRotatedRect(Painter& paint, Float2 center, Float2 half, float radians, const ShapeStyle& style);
// 'alpha' replaces the colours' alpha (fading the whole bolt).
void DrawLightning(Painter& paint, Float2 from, Float2 to, int seed, Color4 color, float alpha,
                   const LightningStyle& style);
// Tab border: the top and both sides; the bottom stays open where the tab joins its panel.
void DrawTabOutline(Painter& paint, const FloatRect& tab, float thickness, Color4 color);
// Panel border whose top edge stays open under the active tab.
void DrawTabbedPanelOutline(Painter& paint, const FloatRect& panel, const FloatRect& activeTab, float thickness,
                            Color4 color);
