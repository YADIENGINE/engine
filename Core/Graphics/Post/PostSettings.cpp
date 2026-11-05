#include "Graphics/Post/PostSettings.h"
#include <algorithm>

namespace {
[[nodiscard]] float Between(float from, float to, float t) { return from + (to - from) * t; }

// A setting of an effect: an effect that is off on one side has no say in it.
[[nodiscard]] float Shape(float from, float to, bool fromOn, bool toOn, float t) {
    if (!fromOn) return to;
    if (!toOn) return from;
    return Between(from, to, t);
}

[[nodiscard]] float ScanlineCount(const PostSettings& from, const PostSettings& to, float t) {
    const bool bothCounted = from.scanlineCount > 0.0f && to.scanlineCount > 0.0f;
    const bool bothOn = from.scanlines > 0.0f && to.scanlines > 0.0f;
    if (bothOn && !bothCounted) return t < 0.5f ? from.scanlineCount : to.scanlineCount;
    return Shape(from.scanlineCount, to.scanlineCount, from.scanlines > 0.0f, to.scanlines > 0.0f, t);
}

[[nodiscard]] Color4 TintColor(const TintSettings& from, const TintSettings& to, float t) {
    if (!(from.amount > 0.0f)) return to.color;
    if (!(to.amount > 0.0f)) return from.color;
    return Mix(from.color, to.color, t);
}
} // namespace

PostSettings Mix(const PostSettings& from, const PostSettings& to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PostSettings mixed;
    mixed.bloom.intensity = Between(from.bloom.intensity, to.bloom.intensity, t);
    mixed.bloom.radius =
        Shape(from.bloom.radius, to.bloom.radius, from.bloom.intensity > 0.0f, to.bloom.intensity > 0.0f, t);
    mixed.vignette = Between(from.vignette, to.vignette, t);
    mixed.scanlines = Between(from.scanlines, to.scanlines, t);
    mixed.scanlineCount = ScanlineCount(from, to, t);
    mixed.aberration = Between(from.aberration, to.aberration, t);
    mixed.fade = Between(from.fade, to.fade, t);
    mixed.tint.amount = Between(from.tint.amount, to.tint.amount, t);
    mixed.tint.color = TintColor(from.tint, to.tint, t);
    mixed.grain.amount = Between(from.grain.amount, to.grain.amount, t);
    mixed.grain.size = Shape(from.grain.size, to.grain.size, from.grain.amount > 0.0f, to.grain.amount > 0.0f, t);
    mixed.wobble.amount = Between(from.wobble.amount, to.wobble.amount, t);
    mixed.wobble.speed =
        Shape(from.wobble.speed, to.wobble.speed, from.wobble.amount > 0.0f, to.wobble.amount > 0.0f, t);
    mixed.tape.band = Between(from.tape.band, to.tape.band, t);
    mixed.tape.shift = Between(from.tape.shift, to.tape.shift, t);
    mixed.tape.speed = Shape(from.tape.speed, to.tape.speed, from.tape.band > 0.0f, to.tape.band > 0.0f, t);
    return mixed;
}
