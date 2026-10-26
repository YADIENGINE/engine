#include "Graphics/Ui/UiDraw.h"
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Draw/TextRenderer.h"
#include "Graphics/Style/Theme.h"

namespace {
const UiStyle& Ui() { return CurrentTheme().ui; }
} // namespace

void DrawUiFrame(Rectangle rect, float notch, float thickness, Color color, float glow) {
    DrawNeonFrame(rect, notch, thickness, color, glow, Ui().frame);
}
void DrawUiText(const TextRenderer& text, const char* label, int centerX, int y, int fontSize, Color color,
                int maxWidth, float glow) {
    DrawNeonText(text, label, centerX, y, fontSize, color, maxWidth, glow, Ui().text);
}
void DrawUiPanel(const Rect& rect, Color accent) { DrawNeonPanel(rect, accent, Ui().panel); }
void DrawUiChip(const TextRenderer& text, const char* label, Vector2 center, int fontSize, Color color) {
    DrawNeonChip(text, label, center, fontSize, color, Ui().chip);
}
Rectangle DrawUiButtonBody(const Rect& rect, const ButtonLook& look, float time) {
    return DrawArcadeButtonBody(rect, look, time, Ui().button);
}
void DrawUiButton(const Rect& rect, const char* label, int fontSize, const ButtonLook& look, const TextRenderer& text,
                  float time) {
    DrawArcadeButton(rect, label, fontSize, look, text, time, Ui().button);
}
void DrawUiRoundButton(const Rect& rect, const char* label, int fontSize, Color accent, const TextRenderer& text) {
    DrawRoundButton(rect, label, fontSize, accent, text, Ui().roundButton);
}
void DrawUiKeyCap(const Rect& area, const char* key, int fontSize, const TextRenderer& text) {
    const KeyCapStyle& style = Ui().keyCap;
    const Rectangle rect = ToRectangle(area);
    DrawRectangleRounded(rect, style.roundness, style.segments, style.body);
    DrawRectangleRoundedLinesEx(rect, style.roundness, style.segments, style.frameWidth, style.frame);
    text.DrawCentered(key, area.CenterX(), area.CenterY() - fontSize / 2, fontSize, style.label);
}
void DrawUiProgressBar(Rectangle bar, const ProgressBarLook& look) { DrawProgressBar(bar, look, Ui().progressBar); }
