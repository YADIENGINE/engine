#include "Ui/Looks/UiTooltipLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

namespace {
constexpr float Radius = 4.0f;
constexpr float FrameWidth = 1.0f;
} // namespace

void DrawPlainTooltip(Painter& paint, const UiDrawState& state) {
    const ThemePalette& palette = CurrentTheme().palette;
    paint.RoundRect(state.rect, Radius, Fill(palette.background));
    paint.RoundRect(state.rect, Radius, Outline(FrameWidth, palette.surfaceFrame, StrokeAlign::Inside));
}
