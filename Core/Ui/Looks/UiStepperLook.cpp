#include "Ui/Looks/UiStepperLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr float ArrowSize = 7.0f;
constexpr float ArrowInset = 8.0f;
constexpr float ArrowsWidth = 2.0f * (ArrowSize + ArrowInset);
constexpr float DisabledOpacity = 0.45f;
} // namespace

void DrawPlainStepper(Painter& paint, const UiStepperView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawPlainControlRow(paint, view.control);
    const ThemePalette& palette = CurrentTheme().palette;
    const UiDrawState& state = view.control.state;
    const FloatRect area = view.control.area;
    const float middle = area.Center().y;
    const Color4 arrow = state.focused ? view.control.accent.value_or(palette.accent) : palette.textDim;
    const float left = area.x + ArrowInset;
    const float right = area.Right() - ArrowInset;
    paint.Triangle({left, middle}, {left + ArrowSize, middle - ArrowSize}, {left + ArrowSize, middle + ArrowSize},
                   Fill(arrow));
    paint.Triangle({right, middle}, {right - ArrowSize, middle + ArrowSize}, {right - ArrowSize, middle - ArrowSize},
                   Fill(arrow));
    if (!state.font) return;
    const float size = view.control.textSize;
    paint.Text(*state.font, view.valueText, {area.Center().x, middle - state.font->LineHeight(size) * 0.5f},
               CenteredText(size, palette.text, area.width - 2.0f * ArrowsWidth));
}
