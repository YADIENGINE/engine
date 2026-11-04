#include "Fx/Screen/Announcements.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Animation/Easing.h"
#include "Graphics/Paint/Painter.h"

namespace {
void ValidateTiming(float duration, float fade) {
    if (!std::isfinite(duration) || duration <= 0.0f || !std::isfinite(fade) || fade < 0.0f) {
        throw std::invalid_argument("Invalid announcement timing");
    }
}
Float2 Position(const FloatRect& bounds, Float2 position, Float2 offset) {
    return {bounds.x + bounds.width * position.x + offset.x, bounds.y + bounds.height * position.y + offset.y};
}
FxTextStyle BoundedText(FxTextStyle style, const FloatRect& bounds) {
    if (style.maxWidth <= 0.0f) style.maxWidth = bounds.width;
    return style;
}
} // namespace

Announcements::Announcements(std::size_t toastCapacity)
    : _toastCapacity(toastCapacity) {}
void Announcements::Banner(const std::string& text, const BannerStyle& style) {
    ValidateTiming(style.duration, style.fadeSeconds);
    if (!std::isfinite(style.curtainFadeSeconds) || style.curtainFadeSeconds < 0.0f || !(style.text.size > 0.0f)) {
        throw std::invalid_argument("Invalid banner style");
    }
    BannerEntry entry{text, style, SecondsTween<float>{1.0f}, SecondsTween<float>{0.0f}};
    const float fade = std::min(style.fadeSeconds, style.duration);
    entry.alpha.Wait(style.duration - fade).Then(0.0f, fade);
    if (style.curtainFadeSeconds > 0.0f) {
        entry.curtain.Snap(1.0f);
        entry.curtain.To(0.0f, style.curtainFadeSeconds);
    }
    _banner = std::move(entry);
}
void Announcements::Toast(const std::string& text, Color4 color, const ToastStyle& style) {
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
void Announcements::ShowToast(ToastEntry entry) {
    entry.life = _toastQueue.empty() ? entry.style.duration : entry.style.queuedDuration;
    _toast = std::move(entry);
}
void Announcements::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    if (_banner) {
        UpdateAll(deltaSeconds, _banner->alpha, _banner->curtain);
        if (!_banner->alpha.Active()) _banner.reset();
    }
    if (_toast && (_toast->life -= deltaSeconds) <= 0.0f) _toast.reset();
    if (!_toast && !_toastQueue.empty()) {
        ToastEntry next = std::move(_toastQueue.front());
        _toastQueue.pop_front();
        ShowToast(std::move(next));
    }
}
void Announcements::Clear() {
    _banner.reset();
    _toast.reset();
    _toastQueue.clear();
}
void Announcements::Draw(Painter& paint, const FloatRect& bounds, const FloatRect& fillBounds) const {
    if (bounds.Empty()) return;
    if (_banner) {
        const BannerStyle& style = _banner->style;
        if (_banner->curtain.Value() > 0.0f)
            paint.Rect(fillBounds, Fill(WithAlpha(style.curtain, _banner->curtain.Value())));
        DrawFxText(paint, _banner->text, Position(bounds, style.position, style.offset),
                   WithAlpha(style.color, _banner->alpha.Value()), BoundedText(style.text, bounds));
    }
    if (_toast) {
        const ToastStyle& style = _toast->style;
        const float alpha = style.fadeSeconds > 0.0f ? Clamp01(_toast->life / style.fadeSeconds) : 1.0f;
        DrawFxText(paint, _toast->text, Position(bounds, style.position, style.offset), WithAlpha(_toast->color, alpha),
                   BoundedText(style.text, bounds));
    }
}
