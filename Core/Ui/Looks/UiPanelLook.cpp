#include "Ui/Looks/UiPanelLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

namespace {
constexpr float Radius = 6.0f;
constexpr float FrameWidth = 1.0f;
} // namespace

void DrawPlainPanel(Painter& paint, const UiPanelView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    paint.RoundRect(view.state.rect, Radius, Fill(palette.surface));
    const Color4 frame = view.accent.value_or(palette.surfaceFrame);
    paint.RoundRect(view.state.rect, Radius, Outline(FrameWidth, frame, StrokeAlign::Inside));
}
