#include "Fx/PulseOverlay2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Utilities/MathUtils.h"

PulseOverlay2D::PulseOverlay2D(const PulseOverlayStyle& style)
    : _style(style) {}

void PulseOverlay2D::SetActive(bool active, Color color) {
    _active = active;
    _color = color;
}
void PulseOverlay2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    const float target = _active ? 1.0f : 0.0f;
    const float step = _style.fadeSpeed * deltaSeconds;
    _intensity = _intensity < target ? std::min(_intensity + step, target) : std::max(_intensity - step, target);
    _time = _intensity > 0.0f ? std::fmod(_time + deltaSeconds, _style.period) : 0.0f;
}
void PulseOverlay2D::Clear() {
    _active = false;
    _intensity = 0.0f;
    _time = 0.0f;
    _color = {};
}
float PulseOverlay2D::Heartbeat() const {
    const auto beat = [this](float t) { return t < 0.0f ? 0.0f : std::exp(-t * _style.decay); };
    return beat(_time) + _style.secondBeatScale * beat(_time - _style.secondBeatAt);
}

void PulseOverlay2D::DrawTint(const Rect& area) const {
    if (_intensity <= 0.0f) return;
    BeginBlendMode(BLEND_ADDITIVE);
    FillRect(area, Fade(_color, _intensity * (_style.tintAlpha + _style.tintBeatAlpha * Heartbeat())));
    EndBlendMode();
}

void PulseOverlay2D::DrawVignette(const Rect& area) const {
    if (_intensity <= 0.0f) return;
    const Color edge = Fade(_color, _intensity * (_style.vignetteAlpha + _style.vignetteBeatAlpha * Heartbeat()));
    const Color clear = Fade(_color, 0.0f);
    DrawRectangleGradientV(area.x, area.y, area.width, _style.vignetteSize, edge, clear);
    DrawRectangleGradientV(area.x, area.Bottom() - _style.vignetteSize, area.width, _style.vignetteSize, clear, edge);
    DrawRectangleGradientH(area.x, area.y, _style.vignetteSize, area.height, edge, clear);
    DrawRectangleGradientH(area.Right() - _style.vignetteSize, area.y, _style.vignetteSize, area.height, clear, edge);
}
