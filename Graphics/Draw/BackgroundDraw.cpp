#include "Graphics/Draw/BackgroundDraw.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Draw/EffectQuality.h"
#include "Graphics/Render/Internals/RenderTargetStack.h"

namespace {
// İnce çizgi piksel ızgarasına oturur: kesirli ölçekte 1 birimlik çizgiler 1 ve 2 piksel arasında oynamaz.
void FillLine(int x, int y, int width, int height, Color color) {
    DrawRectangleRec(AlignToPixels({static_cast<float>(x), static_cast<float>(y), static_cast<float>(width),
                                    static_cast<float>(height)}),
                     color);
}
} // namespace

void DrawGrid2D(const Rect& bounds, int spacing, Vector2 offset, Color color, int thickness) {
    if (spacing <= 0) throw std::invalid_argument("Grid spacing must be positive");
    if (bounds.width <= 0 || bounds.height <= 0 || thickness <= 0) return;
    const int shiftX = static_cast<int>(std::fmod(offset.x, static_cast<float>(spacing)) + static_cast<float>(spacing));
    const int shiftY = static_cast<int>(std::fmod(offset.y, static_cast<float>(spacing)) + static_cast<float>(spacing));
    for (int x = bounds.x + shiftX % spacing; x < bounds.Right(); x += spacing) {
        FillLine(x, bounds.y, std::min(thickness, bounds.Right() - x), bounds.height, color);
    }
    for (int y = bounds.y + shiftY % spacing; y < bounds.Bottom(); y += spacing) {
        FillLine(bounds.x, y, bounds.width, std::min(thickness, bounds.Bottom() - y), color);
    }
}
void DrawScanlines(const Rect& bounds, Color color, int spacing, int thickness) {
    if (spacing <= 0) throw std::invalid_argument("Scanline spacing must be positive");
    if (bounds.width <= 0 || bounds.height <= 0 || thickness <= 0) return;
    if (!CurrentEffectQuality().backgroundEffects) return;
    for (int y = bounds.y; y < bounds.Bottom(); y += spacing) {
        FillLine(bounds.x, y, bounds.width, std::min(thickness, bounds.Bottom() - y), color);
    }
}
void DrawVignette(const Rect& bounds, int size, Color color) {
    if (bounds.width <= 0 || bounds.height <= 0 || size <= 0) return;
    const Color clear = Fade(color, 0.0f);
    DrawRectangleGradientV(bounds.x, bounds.y, bounds.width, size, color, clear);
    DrawRectangleGradientV(bounds.x, bounds.Bottom() - size, bounds.width, size, clear, color);
    DrawRectangleGradientH(bounds.x, bounds.y, size, bounds.height, color, clear);
    DrawRectangleGradientH(bounds.Right() - size, bounds.y, size, bounds.height, clear, color);
}
void DrawScanlines(const Rect& bounds, const ScanlineStyle& style, float alpha) {
    DrawScanlines(bounds, Fade(style.color, alpha), style.spacing, style.thickness);
}

void DrawBackdropGrid(const BackdropStyle& style, const Rect& area, float time) {
    const float scroll = CurrentEffectQuality().backgroundEffects ? time * style.gridScrollSpeed : 0.0f;
    DrawGrid2D(area, style.gridCell, {0.0f, scroll}, style.gridColor, 1);
}
void DrawBackdropVignette(const BackdropStyle& style, const Rect& area) {
    DrawVignette(area, style.vignetteSize, style.vignetteColor);
}
void DrawBackdropScanlines(const BackdropStyle& style, const ScanlineStyle& scanlines, const Rect& area) {
    DrawScanlines(area, scanlines, style.scanlineAlpha);
}
