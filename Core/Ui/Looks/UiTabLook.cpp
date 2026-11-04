#include "Ui/Looks/UiTabLook.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr float Radius = 4.0f;
constexpr float UnderlineHeight = 3.0f;
constexpr float CursorWidth = 2.0f;
constexpr float BodyTint = 0.18f;
constexpr float LabelInset = 6.0f;
} // namespace

void DrawPlainTab(Painter& paint, const UiTabView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.accent.value_or(palette.accent);
    const UiDrawState& state = view.state;
    const float highlight = std::clamp(view.animation.highlight, 0.0f, 1.0f);
    paint.RoundRect(state.rect, Radius, Fill(Mix(palette.surface, accent, highlight * BodyTint)));
    if (view.selected) {
        const FloatRect line{state.rect.x, state.rect.Bottom() - UnderlineHeight, state.rect.width, UnderlineHeight};
        paint.Rect(line, Fill(accent));
    }
    if (view.cursor) {
        const Color4 cursor = state.cursorColor.a == 0 ? accent : state.cursorColor;
        paint.RoundRect(state.rect, Radius, Outline(CursorWidth, cursor, StrokeAlign::Inside));
    }
    if (!state.font) return;
    const Color4 text = view.selected ? palette.text : palette.textSoft;
    const Float2 center = state.rect.Center();
    paint.Text(*state.font, view.label, {center.x, center.y - state.font->LineHeight(view.textSize) * 0.5f},
               CenteredText(view.textSize, text, state.rect.width - 2.0f * LabelInset));
}
