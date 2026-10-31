#include "Fx/FloatingText2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Anim/Easing.h"

void FloatingText2D::Emit(Float2 position, const std::string& text, Color4 color, const FloatingTextStyle& style) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f || !std::isfinite(style.fadeSeconds) ||
        style.fadeSeconds < 0.0f || !std::isfinite(style.popSeconds) || style.popSeconds < 0.0f ||
        !(style.text.size > 0.0f) || !std::isfinite(style.velocity.x) || !std::isfinite(style.velocity.y)) {
        throw std::invalid_argument("Invalid floating text style");
    }
    _entries.push_back({position, text, color, style.duration, style});
}

void FloatingText2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Entry& entry : _entries) {
        entry.position += entry.style.velocity * deltaSeconds;
        entry.life -= deltaSeconds;
    }
    std::erase_if(_entries, [](const Entry& entry) { return entry.life <= 0.0f; });
}

void FloatingText2D::Clear() { _entries.clear(); }

void FloatingText2D::Draw(Painter& paint) const {
    for (const Entry& entry : _entries) {
        FxTextStyle text = entry.style.text;
        if (entry.style.popSeconds > 0.0f)
            text.size *= EaseOutBack((entry.style.duration - entry.life) / entry.style.popSeconds);
        if (!(text.size > 0.0f)) continue;
        const float alpha = entry.style.fadeSeconds > 0.0f ? Clamp01(entry.life / entry.style.fadeSeconds) : 1.0f;
        DrawFxText(paint, entry.text, entry.position, WithAlpha(entry.color, alpha), text);
    }
}
