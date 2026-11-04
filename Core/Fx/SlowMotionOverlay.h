#pragma once
#include "Animation/Curve.h"
#include "Animation/Tween.h"
#include "Fx/FxQuality.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"

class Painter;

// The look of slow motion: tints the area, darkens the edges and streams slow light trails; fades after 'fadeStart' of
// its duration.
struct SlowMotionStyle {
    float fadeStart = 1.0f;
    float tintAlpha = 0.0f;
    float vignetteSize = 0.0f;
    float vignetteAlpha = 0.0f;
    SpeedLineStyle lines;
    Curve fadeCurve = Curve::OutCubic;
};

// Only the look: time itself is TimeScale's. Its trails run on Update time, so they crawl while time is slowed.
class SlowMotionOverlay {
public:
    // Flashes off: no tint or dark edges. Motion off: no trails.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // Invalid times throw invalid_argument.
    void Trigger(float seconds, Color4 color, const SlowMotionStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint, const FloatRect& area, const FloatRect& viewArea) const;

private:
    FxQuality _quality;
    SlowMotionStyle _style;
    Color4 _color{};
    float _age = 0.0f;
    SecondsTween<float> _strength;
};
