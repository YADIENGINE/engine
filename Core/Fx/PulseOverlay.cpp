#include "Fx/PulseOverlay2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Paint/Painter.h"

PulseOverlay2D::PulseOverlay2D(const PulseOverlayStyle& style)
    : _style(style) {}

void PulseOverlay2D::SetActive(bool active, Color4 color) {
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

void PulseOverlay2D::DrawTint(Painter& paint, const FloatRect& area) const {
    if (_intensity <= 0.0f) return;
    auto light = paint.Blending(Blend::Add);
    paint.Rect(area, Fill(WithAlpha(_color, _intensity * (_style.tintAlpha + _style.tintBeatAlpha * Heartbeat()))));
}

void PulseOverlay2D::DrawVignette(Painter& paint, const FloatRect& area) const {
    if (_intensity <= 0.0f) return;
    ::DrawVignette(paint, area, _style.vignetteSize,
                   WithAlpha(_color, _intensity * (_style.vignetteAlpha + _style.vignetteBeatAlpha * Heartbeat())));
}
