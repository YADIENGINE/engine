#include "Fx/Screen/PulseOverlay.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Paint/Painter.h"

bool IsValid(const PulseOverlayStyle& style) {
    return FxPositive(style.period) && FxPositive(style.fadeSpeed) && FxNotNegative(style.decay) &&
           FxNotNegative(style.secondBeatAt) && FxNotNegative(style.secondBeatScale) &&
           FxNotNegative(style.tintAlpha) && FxNotNegative(style.tintBeatAlpha) && FxNotNegative(style.vignetteSize) &&
           FxNotNegative(style.vignetteAlpha) && FxNotNegative(style.vignetteBeatAlpha);
}

PulseOverlay::PulseOverlay(const PulseOverlayStyle& style)
    : _style(style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid pulse style");
}

void PulseOverlay::SetActive(bool active, Color4 color) {
    _active = active;
    _color = color;
}
void PulseOverlay::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    const float target = _active ? 1.0f : 0.0f;
    const float step = _style.fadeSpeed * deltaSeconds;
    _intensity = _intensity < target ? std::min(_intensity + step, target) : std::max(_intensity - step, target);
    _time = _intensity > 0.0f ? std::fmod(_time + deltaSeconds, _style.period) : 0.0f;
}
void PulseOverlay::Clear() {
    _active = false;
    _intensity = 0.0f;
    _time = 0.0f;
    _color = {};
}
float PulseOverlay::Heartbeat() const {
    if (!_quality.flashes) return 0.0f;
    const auto beat = [this](float t) { return t < 0.0f ? 0.0f : std::exp(-t * _style.decay); };
    return beat(_time) + _style.secondBeatScale * beat(_time - _style.secondBeatAt);
}

void PulseOverlay::DrawTint(Painter& paint, const FloatRect& area) const {
    if (_intensity <= 0.0f) return;
    auto light = paint.Blending(Blend::Add);
    paint.Rect(area, Fill(WithAlpha(_color, _intensity * (_style.tintAlpha + _style.tintBeatAlpha * Heartbeat()))));
}

void PulseOverlay::DrawVignette(Painter& paint, const FloatRect& area) const {
    if (_intensity <= 0.0f) return;
    ::DrawVignette(paint, area, _style.vignetteSize,
                   WithAlpha(_color, _intensity * (_style.vignetteAlpha + _style.vignetteBeatAlpha * Heartbeat())));
}
