#include "Ui/Looks/UiScrollLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

void DrawPlainScrollBar(Painter& paint, const UiScrollView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const float radius = view.state.rect.width * 0.5f;
    paint.RoundRect(view.state.rect, radius, Fill(ScaleAlpha(palette.surfaceFrame, 0.4f)));
    const bool lit = view.dragging || view.state.hovered;
    paint.RoundRect(view.thumb, radius, Fill(lit ? palette.accent : palette.textDim));
}
