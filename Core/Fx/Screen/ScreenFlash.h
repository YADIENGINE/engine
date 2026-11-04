#pragma once
#include "Animation/Curve.h"
#include "Animation/Tween.h"
#include "Fx/FxQuality.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

class Painter;

struct FlashStyle {
    float duration = 1.0f;
    float opacity = 1.0f;
    Curve curve = Curve::Linear; // How it fades
};

[[nodiscard]] bool IsValid(const FlashStyle& style);

// A colour over the whole view that fades away. A new flash takes over unless the one showing is still brighter. For
// players sensitive to light, flashes start at most three times a second, on Update time; one asked for sooner does not
// happen.
class ScreenFlash {
public:
    static constexpr float MinSecondsBetween = 1.0f / 3.0f;
    // Flashes off: Flash does nothing.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // An invalid style throws invalid_argument.
    void Flash(Color4 color, const FlashStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint, const FloatRect& bounds) const;

private:
    FxQuality _quality;
    Color4 _color{};
    float _opacity = 0.0f;
    float _untilNext = 0.0f;
    SecondsTween<float> _strength;
};
