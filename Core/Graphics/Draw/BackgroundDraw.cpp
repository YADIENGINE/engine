#include "Graphics/Draw/BackgroundDraw.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"

void DrawGrid2D(Painter& paint, const FloatRect& bounds, float spacing, Float2 offset, Color4 color, float thickness) {
    if (!(spacing > 0.0f)) return paint.Reject("Grid spacing must be positive");
    if (bounds.Empty() || !(thickness > 0.0f) || color.a == 0) return;
    const float shiftX = std::fmod(std::fmod(offset.x, spacing) + spacing, spacing);
    const float shiftY = std::fmod(std::fmod(offset.y, spacing) + spacing, spacing);
    for (float x = bounds.x + shiftX; x < bounds.Right(); x += spacing)
        paint.Rect({x, bounds.y, std::min(thickness, bounds.Right() - x), bounds.height}, Fill(color));
    for (float y = bounds.y + shiftY; y < bounds.Bottom(); y += spacing)
        paint.Rect({bounds.x, y, bounds.width, std::min(thickness, bounds.Bottom() - y)}, Fill(color));
}

void DrawScanlines(Painter& paint, const FloatRect& bounds, Color4 color, float spacing, float thickness) {
    if (!(spacing > 0.0f)) return paint.Reject("Scanline spacing must be positive");
    if (bounds.Empty() || !(thickness > 0.0f) || color.a == 0 || !paint.Quality().backgroundEffects) return;
    for (float y = bounds.y; y < bounds.Bottom(); y += spacing)
        paint.Rect({bounds.x, y, bounds.width, std::min(thickness, bounds.Bottom() - y)}, Fill(color));
}

void DrawScanlines(Painter& paint, const FloatRect& bounds, const ScanlineStyle& style, float alpha) {
    DrawScanlines(paint, bounds, WithAlpha(style.color, alpha), style.spacing, style.thickness);
}

void DrawVignette(Painter& paint, const FloatRect& bounds, float size, Color4 color) {
    if (bounds.Empty() || !(size > 0.0f) || color.a == 0) return;
    const Color4 clear = WithAlpha(color, 0.0f);
    const float across = std::min(size, std::min(bounds.width, bounds.height));
    const auto band = [&](const FloatRect& rect, Float2 direction) {
        // 'direction' points from the edge inwards; the gradient is local to the band's centre.
        const Float2 half{direction.x * rect.width * 0.5f, direction.y * rect.height * 0.5f};
        paint.Rect(rect, {.gradient = Gradient::Linear(-half, half, color, clear)});
    };
    band({bounds.x, bounds.y, bounds.width, across}, {0.0f, 1.0f});
    band({bounds.x, bounds.Bottom() - across, bounds.width, across}, {0.0f, -1.0f});
    band({bounds.x, bounds.y, across, bounds.height}, {1.0f, 0.0f});
    band({bounds.Right() - across, bounds.y, across, bounds.height}, {-1.0f, 0.0f});
}

void DrawBackdropGrid(Painter& paint, const BackdropStyle& style, const FloatRect& area) {
    // Wrapped in double: the presentation clock grows without bound.
    const double scroll = paint.Quality().backgroundEffects && style.gridCell > 0.0f
                              ? std::fmod(paint.Time() * style.gridScrollSpeed, static_cast<double>(style.gridCell))
                              : 0.0;
    DrawGrid2D(paint, area, style.gridCell, {0.0f, static_cast<float>(scroll)}, style.gridColor, 1.0f);
}

void DrawBackdropVignette(Painter& paint, const BackdropStyle& style, const FloatRect& area) {
    DrawVignette(paint, area, style.vignetteSize, style.vignetteColor);
}

void DrawBackdropScanlines(Painter& paint, const BackdropStyle& style, const ScanlineStyle& scanlines,
                           const FloatRect& area) {
    DrawScanlines(paint, area, scanlines, style.scanlineAlpha);
}
