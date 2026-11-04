#include "Ui/Looks/UiToggleLook.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

namespace {
constexpr float SwitchHeight = 24.0f;
constexpr float KnobInset = 3.0f;
constexpr float DisabledOpacity = 0.45f;
} // namespace

void DrawPlainToggle(Painter& paint, const UiToggleView& view) {
    auto faded = paint.Opacity(view.control.state.enabled ? 1.0f : DisabledOpacity);
    DrawPlainControlRow(paint, view.control);
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.control.accent.value_or(palette.accent);
    const FloatRect track = FloatRect::FromCenter(view.control.area.Center(), {view.control.area.width, SwitchHeight});
    const float on = std::clamp(view.on, 0.0f, 1.0f);
    paint.RoundRect(track, SwitchHeight * 0.5f, Fill(Mix(palette.surfaceFrame, accent, on)));
    const float radius = SwitchHeight * 0.5f - KnobInset;
    const float travel = track.width - 2.0f * (radius + KnobInset);
    paint.Circle({track.x + radius + KnobInset + travel * on, track.Center().y}, radius, Fill(palette.text));
}
