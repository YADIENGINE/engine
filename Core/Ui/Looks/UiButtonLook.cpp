#include "Ui/Looks/UiButtonLook.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"

namespace {
constexpr float Radius = 4.0f;
constexpr float FrameWidth = 1.0f;
constexpr float CursorWidth = 2.0f;
constexpr float SecondCursorGap = 3.0f;
constexpr float BodyTint = 0.18f;
constexpr float PressInset = 2.0f;
constexpr float ShakeAmount = 4.0f;
constexpr float ShakeTurns = 24.0f;
constexpr float DisabledOpacity = 0.45f;
constexpr float LabelInset = 8.0f;

[[nodiscard]] Color4 Or(Color4 cursor, Color4 fallback) { return cursor.a == 0 ? fallback : cursor; }
} // namespace

void DrawPlainButton(Painter& paint, const UiButtonView& view) {
    const ThemePalette& palette = CurrentTheme().palette;
    const Color4 accent = view.accent.value_or(palette.accent);
    const UiDrawState& state = view.state;
    const float highlight = std::clamp(view.animation.highlight, 0.0f, 1.0f);
    const float shake = std::sin(view.animation.shake * ShakeTurns) * view.animation.shake * ShakeAmount;
    const FloatRect rect = state.rect.Translated({shake, 0.0f}).Inset(view.animation.press * PressInset);
    auto faded = paint.Opacity(state.enabled ? 1.0f : DisabledOpacity);

    paint.RoundRect(rect, Radius, Fill(Mix(palette.surface, accent, highlight * BodyTint)));
    const Color4 cursor = Or(state.cursorColor, accent);
    paint.RoundRect(rect, Radius, Outline(FrameWidth, palette.surfaceFrame, StrokeAlign::Inside));
    if (highlight > 0.0f) {
        const float width = CursorWidth * highlight;
        paint.RoundRect(rect, Radius, Outline(width, cursor, StrokeAlign::Inside));
    }
    if (std::popcount(state.focusUsers) > 1) {
        const FloatRect outer = rect.Inset(-SecondCursorGap);
        paint.RoundRect(outer, Radius, Outline(CursorWidth, Or(state.secondCursorColor, accent), StrokeAlign::Inside));
    }
    if (!state.font) return;
    const TextStyle text = CenteredText(view.textSize, palette.text, rect.width - 2.0f * LabelInset);
    const float lineHeight = state.font->LineHeight(view.textSize);
    paint.Text(*state.font, view.label, {rect.Center().x, rect.Center().y - lineHeight * 0.5f}, text);
}
