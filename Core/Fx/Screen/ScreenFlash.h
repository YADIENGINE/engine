#pragma once
#include "Animation/Curve.h"
#include "Animation/Tween.h"
#include "Fx/FxQuality.h"
#include "Fx/Screen/FlashLimit.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

struct FlashStyle {
    float duration = 1.0f;
    float opacity = 1.0f;
    Curve curve = Curve::Linear; // How it fades
};

[[nodiscard]] bool IsValid(const FlashStyle& style);

// A colour over the whole view that fades away. A new flash takes over unless the one showing is still brighter. Starts
// are rationed by a FlashLimit.
class ScreenFlash {
public:
    static constexpr float MinSecondsBetween = FlashLimit::MinSecondsBetween;
    // Flashes off: Flash does nothing.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // An invalid style throws invalid_argument.
    void Flash(Color4 color, const FlashStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint, const FloatRect& bounds) const;
    // What other flashes of the same view share with this one.
    [[nodiscard]] FlashLimit& Limit() { return _limit; }

private:
    FxQuality _quality;
    Color4 _color{};
    float _opacity = 0.0f;
    FlashLimit _limit;
    SecondsTween<float> _strength;
};
