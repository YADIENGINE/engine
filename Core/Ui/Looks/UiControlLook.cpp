#include "Ui/Looks/UiControlLook.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr float Radius = 4.0f;
constexpr float FrameWidth = 1.0f;
constexpr float CursorWidth = 2.0f;
constexpr float BodyTint = 0.14f;
constexpr float LabelInset = 14.0f;
constexpr float LabelGap = 10.0f;
constexpr float ShakeAmount = 4.0f;
constexpr float ShakeTurns = 24.0f;
} // namespace

FloatRect UiControlArea(const FloatRect& row, const UiControlMetrics& metrics) {
    const float room = std::max(row.width - metrics.padding.left - metrics.padding.right, 0.0f);
    const float width = std::min(metrics.controlWidth, room);
    return {row.Right() - metrics.padding.right - width, row.y, width, row.height};
}

void DrawPlainControlRow(Painter& paint, const UiControlView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const UiDrawState& state = view.state;
    const Color4 accent = view.accent.value_or(palette.accent);
    const float highlight = std::clamp(view.animation.highlight, 0.0f, 1.0f);
    const float shake = std::sin(view.animation.shake * ShakeTurns) * view.animation.shake * ShakeAmount;
    const FloatRect rect = state.rect.Translated({shake, 0.0f});
    paint.RoundRect(rect, Radius, Fill(Mix(palette.surface, accent, highlight * BodyTint)));
    paint.RoundRect(rect, Radius, Outline(FrameWidth, palette.surfaceFrame, StrokeAlign::Inside));
    if (highlight > 0.0f) {
        const Color4 cursor = state.cursorColor.a == 0 ? accent : state.cursorColor;
        paint.RoundRect(rect, Radius, Outline(CursorWidth * highlight, cursor, StrokeAlign::Inside));
    }
    if (!state.font) return;
    const float room = view.area.x - rect.x - LabelInset - LabelGap;
    const float lineHeight = state.font->LineHeight(view.textSize);
    paint.Text(*state.font, view.label, {rect.x + LabelInset, rect.Center().y - lineHeight * 0.5f},
               FittedText(view.textSize, palette.text, std::max(room, 1.0f)));
}
