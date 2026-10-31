#include "Fx/OverlayEffects2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Paint/Painter.h"

namespace {
void ValidateTiming(float duration, float fade) {
    if (!std::isfinite(duration) || duration <= 0.0f || !std::isfinite(fade) || fade < 0.0f) {
        throw std::invalid_argument("Invalid overlay timing");
    }
}
float FadeOut(float life, float seconds) { return seconds > 0.0f ? Clamp01(life / seconds) : 1.0f; }
Float2 Position(const FloatRect& bounds, Float2 position, Float2 offset) {
    return {bounds.x + bounds.width * position.x + offset.x, bounds.y + bounds.height * position.y + offset.y};
}
FxTextStyle BoundedText(FxTextStyle style, const FloatRect& bounds) {
    if (style.maxWidth <= 0.0f) style.maxWidth = bounds.width;
    return style;
}
} // namespace

OverlayEffects2D::OverlayEffects2D(std::size_t toastCapacity)
    : _toastCapacity(toastCapacity) {}
void OverlayEffects2D::Flash(Color4 color, const FlashStyle& style) {
    ValidateTiming(style.duration, 0.0f);
    if (!std::isfinite(style.opacity) || style.opacity < 0.0f || style.opacity > 1.0f) {
        throw std::invalid_argument("Invalid flash opacity");
    }
    if (!_quality.flashes) return;
    _flash = FlashEntry{color, style, style.duration};
}
void OverlayEffects2D::Banner(const std::string& text, const BannerStyle& style) {
    ValidateTiming(style.duration, style.fadeSeconds);
    if (!std::isfinite(style.curtainFadeSeconds) || style.curtainFadeSeconds < 0.0f || !(style.text.size > 0.0f)) {
        throw std::invalid_argument("Invalid banner style");
    }
    _banner = BannerEntry{text, style, style.duration};
}
void OverlayEffects2D::Toast(const std::string& text, Color4 color, const ToastStyle& style) {
    ValidateTiming(style.duration, style.fadeSeconds);
    ValidateTiming(style.queuedDuration, style.fadeSeconds);
    if (!(style.text.size > 0.0f)) throw std::invalid_argument("Invalid toast font size");
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
void OverlayEffects2D::Draw(Painter& paint, const FloatRect& bounds, const FloatRect& fillBounds) const {
    if (bounds.Empty()) return;
    if (_flash)
        paint.Rect(fillBounds,
                   Fill(WithAlpha(_flash->color, _flash->style.opacity * (_flash->life / _flash->style.duration))));
    if (_banner) {
        const BannerStyle& style = _banner->style;
        const float elapsed = style.duration - _banner->life;
        if (elapsed < style.curtainFadeSeconds)
            paint.Rect(fillBounds, Fill(WithAlpha(style.curtain, 1.0f - elapsed / style.curtainFadeSeconds)));
        DrawFxText(paint, _banner->text, Position(bounds, style.position, style.offset),
                   WithAlpha(style.color, FadeOut(_banner->life, style.fadeSeconds)), BoundedText(style.text, bounds));
    }
    if (_toast) {
        const ToastStyle& style = _toast->style;
        DrawFxText(paint, _toast->text, Position(bounds, style.position, style.offset),
                   WithAlpha(_toast->color, FadeOut(_toast->life, style.fadeSeconds)), BoundedText(style.text, bounds));
    }
}
