#pragma once
#include "Animation/Easing.h"

// The curves of Easing.h as a value, for what has to store one: a key, a tween, data. A value can go in a simulation's
// State, where a function pointer could not. Hold stays at the start until the very end, then jumps.
enum class Curve {
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
    InQuart,
    OutQuart,
    InOutQuart,
    InSine,
    OutSine,
    InOutSine,
    InExpo,
    OutExpo,
    InBack,
    OutBack,
    InOutBack,
    InElastic,
    OutElastic,
    InBounce,
    OutBounce,
    Hold
};

[[nodiscard]] inline float Ease(Curve curve, float t) {
    switch (curve) {
    case Curve::Linear    : return EaseLinear(t);
    case Curve::InQuad    : return EaseInQuad(t);
    case Curve::OutQuad   : return EaseOutQuad(t);
    case Curve::InOutQuad : return EaseInOutQuad(t);
    case Curve::InCubic   : return EaseInCubic(t);
    case Curve::OutCubic  : return EaseOutCubic(t);
    case Curve::InOutCubic: return EaseInOutCubic(t);
    case Curve::InQuart   : return EaseInQuart(t);
    case Curve::OutQuart  : return EaseOutQuart(t);
    case Curve::InOutQuart: return EaseInOutQuart(t);
    case Curve::InSine    : return EaseInSine(t);
    case Curve::OutSine   : return EaseOutSine(t);
    case Curve::InOutSine : return EaseInOutSine(t);
    case Curve::InExpo    : return EaseInExpo(t);
    case Curve::OutExpo   : return EaseOutExpo(t);
    case Curve::InBack    : return EaseInBack(t);
    case Curve::OutBack   : return EaseOutBack(t);
    case Curve::InOutBack : return EaseInOutBack(t);
    case Curve::InElastic : return EaseInElastic(t);
    case Curve::OutElastic: return EaseOutElastic(t);
    case Curve::InBounce  : return EaseInBounce(t);
    case Curve::OutBounce : return EaseOutBounce(t);
    case Curve::Hold      : return t >= 1.0f ? 1.0f : 0.0f;
    }
    return EaseLinear(t);
}
