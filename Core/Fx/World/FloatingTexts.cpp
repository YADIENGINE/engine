#include "Fx/World/FloatingTexts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"
#include "Fx/Internals/FxChecks.h"

bool IsValid(const FloatingTextStyle& style) {
    return FxPositive(style.duration) && FxFinite(style.velocity) && FxNotNegative(style.fadeSeconds) &&
           FxNotNegative(style.popSeconds) && IsValid(style.text);
}

void FloatingTexts::Emit(Float2 position, const std::string& text, Color4 color, const FloatingTextStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid floating text style");
    _entries.push_back({position, text, color, style.duration, style});
}

void FloatingTexts::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Entry& entry : _entries) {
        entry.position += entry.style.velocity * deltaSeconds;
        entry.life -= deltaSeconds;
    }
    std::erase_if(_entries, [](const Entry& entry) { return entry.life <= 0.0f; });
}

void FloatingTexts::Clear() { _entries.clear(); }

void FloatingTexts::Draw(Painter& paint) const {
    for (const Entry& entry : _entries) {
        FxTextStyle text = entry.style.text;
        if (entry.style.popSeconds > 0.0f)
            text.size *= Ease(entry.style.popCurve, (entry.style.duration - entry.life) / entry.style.popSeconds);
        if (!(text.size > 0.0f)) continue;
        const float alpha = entry.style.fadeSeconds > 0.0f ? Clamp01(entry.life / entry.style.fadeSeconds) : 1.0f;
        DrawFxText(paint, entry.text, entry.position, WithAlpha(entry.color, alpha), text);
    }
}
