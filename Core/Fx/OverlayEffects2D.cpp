#include "Fx/OverlayEffects2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/EffectQuality.h"
#include "Graphics/Draw/TextRenderer.h"

namespace {
void ValidateTiming(float duration, float fade) {
    if (!std::isfinite(duration) || duration <= 0.0f || !std::isfinite(fade) || fade < 0.0f) {
        throw std::invalid_argument("Invalid overlay timing");
    }
}
float FadeOut(float life, float seconds) { return seconds > 0.0f ? Clamp01(life / seconds) : 1.0f; }
Vector2 Position(const Rect& bounds, Vector2 position, Vector2 offset) {
    return {static_cast<float>(bounds.x) + static_cast<float>(bounds.width) * position.x + offset.x,
            static_cast<float>(bounds.y) + static_cast<float>(bounds.height) * position.y + offset.y};
}
TextStyle BoundedText(TextStyle style, const Rect& bounds) {
    if (style.maxWidth <= 0) style.maxWidth = bounds.width;
    return style;
}
} // namespace

OverlayEffects2D::OverlayEffects2D(std::size_t toastCapacity)
    : _toastCapacity(toastCapacity) {}
void OverlayEffects2D::Flash(Color color, const FlashStyle& style) {
    ValidateTiming(style.duration, 0.0f);
    if (!std::isfinite(style.opacity) || style.opacity < 0.0f || style.opacity > 1.0f) {
        throw std::invalid_argument("Invalid flash opacity");
    }
    if (!CurrentEffectQuality().flashes) return;
    _flash = FlashEntry{color, style, style.duration};
}
void OverlayEffects2D::Banner(const std::string& text, const BannerStyle& style) {
    ValidateTiming(style.duration, style.fadeSeconds);
    if (!std::isfinite(style.curtainFadeSeconds) || style.curtainFadeSeconds < 0.0f || style.text.fontSize <= 0) {
        throw std::invalid_argument("Invalid banner style");
    }
    _banner = BannerEntry{text, style, style.duration};
}
void OverlayEffects2D::Toast(const std::string& text, Color color, const ToastStyle& style) {
    ValidateTiming(style.duration, style.fadeSeconds);
    ValidateTiming(style.queuedDuration, style.fadeSeconds);
    if (style.text.fontSize <= 0) throw std::invalid_argument("Invalid toast font size");
    ToastEntry entry{text, color, style, style.duration};
    if (!_toast) {
        ShowToast(std::move(entry));
        return;
    }
    if (_toastCapacity == 0) return;
    _toast->life = std::min(_toast->life, _toast->style.queuedDuration);
    _toastQueue.push_back(std::move(entry));
    if (_toastQueue.size() > _toastCapacity) _toastQueue.pop_front();
}
void OverlayEffects2D::ShowToast(ToastEntry entry) {
    entry.life = _toastQueue.empty() ? entry.style.duration : entry.style.queuedDuration;
    _toast = std::move(entry);
}
void OverlayEffects2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    if (_flash && (_flash->life -= deltaSeconds) <= 0.0f) _flash.reset();
    if (_banner && (_banner->life -= deltaSeconds) <= 0.0f) _banner.reset();
    if (_toast && (_toast->life -= deltaSeconds) <= 0.0f) _toast.reset();
    if (!_toast && !_toastQueue.empty()) {
        ToastEntry next = std::move(_toastQueue.front());
        _toastQueue.pop_front();
        ShowToast(std::move(next));
    }
}
void OverlayEffects2D::Clear() {
    _flash.reset();
    _banner.reset();
    _toast.reset();
    _toastQueue.clear();
}
void OverlayEffects2D::Draw(const TextRenderer& textRenderer, const Rect& bounds) const {
    Draw(textRenderer, bounds, bounds);
}
void OverlayEffects2D::Draw(const TextRenderer& textRenderer, const Rect& bounds, const Rect& fillBounds) const {
    if (bounds.width <= 0 || bounds.height <= 0) return;
    if (_flash) {
        Color color = _flash->color;
        color.a = static_cast<unsigned char>(255.0f * _flash->style.opacity * (_flash->life / _flash->style.duration));
        FillRect(fillBounds, color);
    }
    if (_banner) {
        const BannerStyle& style = _banner->style;
        const float elapsed = style.duration - _banner->life;
        if (elapsed < style.curtainFadeSeconds) {
            FillRect(fillBounds, Fade(style.curtain, 1.0f - elapsed / style.curtainFadeSeconds));
        }
        textRenderer.DrawAnchored(_banner->text.c_str(), Position(bounds, style.position, style.offset),
                                  Fade(style.color, FadeOut(_banner->life, style.fadeSeconds)),
                                  BoundedText(style.text, bounds));
    }
    if (_toast) {
        const ToastStyle& style = _toast->style;
        textRenderer.DrawAnchored(_toast->text.c_str(), Position(bounds, style.position, style.offset),
                                  Fade(_toast->color, FadeOut(_toast->life, style.fadeSeconds)),
                                  BoundedText(style.text, bounds));
    }
}
