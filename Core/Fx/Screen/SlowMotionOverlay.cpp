#include "Fx/Screen/SlowMotionOverlay.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Paint/Painter.h"

bool IsValid(const SlowMotionStyle& style) {
    return std::isfinite(style.fadeStart) && FxNotNegative(style.tintAlpha) && FxNotNegative(style.vignetteSize) &&
           FxNotNegative(style.vignetteAlpha) && IsValid(style.lines);
}

void SlowMotionOverlay::Trigger(float seconds, Color4 color, const SlowMotionStyle& style) {
    if (!FxPositive(seconds) || !IsValid(style)) throw std::invalid_argument("Invalid slow motion");
    _style = style;
    _color = color;
    _age = 0.0f;
    const float hold = seconds * std::clamp(style.fadeStart, 0.0f, 1.0f);
    _strength.Snap(1.0f);
    _strength.Wait(hold).Then(0.0f, seconds - hold, style.fadeCurve);
}
void SlowMotionOverlay::Update(float gameSeconds, float realSeconds) {
    if (gameSeconds > 0.0f && std::isfinite(gameSeconds)) _age += gameSeconds;
    if (realSeconds > 0.0f && std::isfinite(realSeconds)) _strength.Update(realSeconds);
}
void SlowMotionOverlay::Clear() {
    _age = 0.0f;
    _strength.Snap(0.0f);
}
void SlowMotionOverlay::Draw(Painter& paint, const FloatRect& area, const FloatRect& viewArea) const {
    if (!_strength.Active()) return;
    const float strength = _strength.Value();
    if (_quality.flashes) {
        paint.Rect(area, Fill(WithAlpha(_color, _style.tintAlpha * strength)));
        DrawVignette(paint, viewArea, _style.vignetteSize, WithAlpha(_color, _style.vignetteAlpha * strength));
    }
    if (_quality.motion) DrawSpeedLines(paint, area, strength, _color, _age, _style.lines);
}
