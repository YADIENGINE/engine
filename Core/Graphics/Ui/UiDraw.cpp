#include "Graphics/Ui/UiDraw.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

namespace {
const UiStyle& Ui() { return CurrentTheme().ui; }
float Time(const Painter& paint) { return static_cast<float>(paint.Time()); }
} // namespace

void DrawUiFrame(Painter& paint, const FloatRect& rect, float notch, float thickness, Color4 color, float glow) {
    DrawNeonFrame(paint, rect, notch, thickness, color, glow, Ui().frame);
}
void DrawUiText(Painter& paint, std::string_view label, Float2 position, const TextStyle& text, float glow) {
    DrawNeonText(paint, label, position, text, glow, Ui().text);
}
void DrawUiPanel(Painter& paint, const FloatRect& rect, Color4 accent) {
    DrawNeonPanel(paint, rect, accent, Ui().panel);
}
void DrawUiChip(Painter& paint, std::string_view label, Float2 center, float size, Color4 color) {
    DrawNeonChip(paint, label, center, size, color, Ui().chip);
}
FloatRect DrawUiButtonBody(Painter& paint, const FloatRect& rect, const ButtonLook& look) {
    return DrawArcadeButtonBody(paint, rect, look, Time(paint), Ui().button);
}
void DrawUiButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, const ButtonLook& look) {
    DrawArcadeButton(paint, rect, label, size, look, Time(paint), Ui().button);
}
void DrawUiRoundButton(Painter& paint, const FloatRect& rect, std::string_view label, float size, Color4 accent) {
    DrawRoundButton(paint, rect, label, size, accent, Ui().roundButton);
}
void DrawUiKeyCap(Painter& paint, const FloatRect& area, std::string_view key, float size) {
    DrawKeyCap(paint, area, key, size, Ui().keyCap);
}
void DrawUiProgressBar(Painter& paint, const FloatRect& bar, const ProgressBarLook& look) {
    DrawProgressBar(paint, bar, look, Ui().progressBar);
}
