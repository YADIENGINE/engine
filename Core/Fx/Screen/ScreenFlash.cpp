#include "Fx/Screen/ScreenFlash.h"
#include <cmath>
#include <stdexcept>
#include "Graphics/Paint/Painter.h"

void ScreenFlash::Flash(Color4 color, const FlashStyle& style) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f || !std::isfinite(style.opacity) ||
        style.opacity < 0.0f || style.opacity > 1.0f) {
        throw std::invalid_argument("Invalid flash style");
    }
    if (!_quality.flashes) return;
    _color = color;
    _opacity = style.opacity;
    _strength.Snap(1.0f);
    _strength.To(0.0f, style.duration, style.curve);
}
void ScreenFlash::Update(float deltaSeconds) {
    if (deltaSeconds > 0.0f && std::isfinite(deltaSeconds)) _strength.Update(deltaSeconds);
}
void ScreenFlash::Clear() { _strength.Snap(0.0f); }
void ScreenFlash::Draw(Painter& paint, const FloatRect& bounds) const {
    if (!_strength.Active() || bounds.Empty()) return;
    paint.Rect(bounds, Fill(WithAlpha(_color, _opacity * _strength.Value())));
}
