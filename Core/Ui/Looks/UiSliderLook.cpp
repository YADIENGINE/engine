#include "Ui/Looks/UiSliderLook.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr float TrackHeight = 6.0f;
constexpr float KnobRadius = 8.0f;
constexpr float DisabledOpacity = 0.45f;
} // namespace

FloatRect UiSliderTrack(const FloatRect& row, const UiSliderLook& look) {
    const FloatRect area = UiControlArea(row, look.metrics);
    return {area.x + KnobRadius, area.y, std::max(area.width - look.valueWidth - 2.0f * KnobRadius, 1.0f), area.height};
}

void DrawPlainSlider(Painter& paint, const UiSliderView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawPlainControlRow(paint, view.control);
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.control.accent.value_or(palette.accent);
    const float middle = view.track.Center().y;
    const FloatRect groove{view.track.x, middle - TrackHeight * 0.5f, view.track.width, TrackHeight};
    const float fraction = std::clamp(view.fraction, 0.0f, 1.0f);
    paint.RoundRect(groove, TrackHeight * 0.5f, Fill(palette.surfaceFrame));
    paint.RoundRect({groove.x, groove.y, groove.width * fraction, groove.height}, TrackHeight * 0.5f, Fill(accent));
    paint.Circle({groove.x + groove.width * fraction, middle}, KnobRadius, Fill(palette.text));
    const UiDrawState& state = view.control.state;
    if (!state.font) return;
    const float size = view.control.textSize * 0.85f;
    const FloatRect area = view.control.area;
    paint.Text(*state.font, view.valueText, {area.Right(), middle - state.font->LineHeight(size) * 0.5f},
               {.size = size, .color = palette.textSoft, .align = TextAlign::Right});
}
