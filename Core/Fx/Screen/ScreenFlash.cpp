#include "Fx/Screen/ScreenFlash.h"
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Paint/Painter.h"

bool IsValid(const FlashStyle& style) {
    return FxPositive(style.duration) && FxNotNegative(style.opacity) && style.opacity <= 1.0f;
}

void ScreenFlash::Flash(Color4 color, const FlashStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid flash style");
    if (!_quality.flashes || _untilNext > 0.0f) return;
    if (_strength.Active() && style.opacity < _opacity * _strength.Value()) return;
    _untilNext = MinSecondsBetween;
    _color = color;
    _opacity = style.opacity;
    _strength.Snap(1.0f);
    _strength.To(0.0f, style.duration, style.curve);
}
void ScreenFlash::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _strength.Update(deltaSeconds);
    _untilNext = CountDown(_untilNext, deltaSeconds);
}
void ScreenFlash::Clear() {
    _strength.Snap(0.0f);
    _untilNext = 0.0f;
}
void ScreenFlash::Draw(Painter& paint, const FloatRect& bounds) const {
    if (!_strength.Active() || bounds.Empty()) return;
    paint.Rect(bounds, Fill(WithAlpha(_color, _opacity * _strength.Value())));
}
