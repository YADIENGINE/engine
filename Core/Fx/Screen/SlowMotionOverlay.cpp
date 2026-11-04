#include "Fx/Screen/SlowMotionOverlay.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Paint/Painter.h"

void SlowMotionOverlay::Trigger(float seconds, Color4 color, const SlowMotionStyle& style) {
    if (!std::isfinite(seconds) || seconds <= 0.0f || !std::isfinite(style.fadeStart)) {
        throw std::invalid_argument("Invalid slow motion");
    }
    _style = style;
    _color = color;
    _age = 0.0f;
    const float hold = seconds * std::clamp(style.fadeStart, 0.0f, 1.0f);
    _strength.Snap(1.0f);
    _strength.Wait(hold).Then(0.0f, seconds - hold, style.fadeCurve);
}
void SlowMotionOverlay::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _age += deltaSeconds;
    _strength.Update(deltaSeconds);
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
