#include "Fx/FloatingText2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/TextRenderer.h"

void FloatingText2D::Emit(Vector2 position, const std::string& text, Color color, const FloatingTextStyle& style) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f || !std::isfinite(style.fadeSeconds) ||
        style.fadeSeconds < 0.0f || !std::isfinite(style.popSeconds) || style.popSeconds < 0.0f ||
        style.text.fontSize <= 0 || !std::isfinite(style.velocity.x) || !std::isfinite(style.velocity.y)) {
        throw std::invalid_argument("Invalid floating text style");
    }
    _entries.push_back({position, text, color, style.duration, style});
}

void FloatingText2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Entry& entry : _entries) {
        entry.position.x += entry.style.velocity.x * deltaSeconds;
        entry.position.y += entry.style.velocity.y * deltaSeconds;
        entry.life -= deltaSeconds;
    }
    std::erase_if(_entries, [](const Entry& entry) { return entry.life <= 0.0f; });
}

void FloatingText2D::Clear() { _entries.clear(); }

void FloatingText2D::Draw(const TextRenderer& textRenderer) const {
    for (const Entry& entry : _entries) {
        TextStyle text = entry.style.text;
        if (entry.style.popSeconds > 0.0f) {
            text.fontSize = static_cast<int>(
                std::lround(static_cast<float>(text.fontSize) *
                            EaseOutBack((entry.style.duration - entry.life) / entry.style.popSeconds)));
        }
        if (text.fontSize <= 0) continue;
        const float alpha = entry.style.fadeSeconds > 0.0f ? Clamp01(entry.life / entry.style.fadeSeconds) : 1.0f;
        textRenderer.DrawAnchored(entry.text.c_str(), entry.position, Fade(entry.color, alpha), text);
    }
}
