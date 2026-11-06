#pragma once
#include "Animation/Curve.h"
#include "Animation/Tween.h"
#include "Fx/FxQuality.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

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

[[nodiscard]] bool IsValid(const SlowMotionStyle& style);

// Only the look: time itself is TimeScale's. How long it lasts is in real seconds, as TimeScale::SlowMo's are, so the
// two end together. Its trails run on game time, so they crawl while time is slowed.
class SlowMotionOverlay {
public:
    // Flashes off: no tint or dark edges. Motion off: no trails.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // Invalid times throw invalid_argument.
    void Trigger(float seconds, Color4 color, const SlowMotionStyle& style);
    // How long it shows: real time, so a hitstop or a pause does not hold it.
    void UpdateReal(float realSeconds);
    // Its trails: game time.
    void Update(float gameSeconds);
    void Clear();
    void Draw(Painter& paint, const FloatRect& area, const FloatRect& viewArea) const;

private:
    FxQuality _quality;
    SlowMotionStyle _style;
    Color4 _color{};
    float _age = 0.0f;
    SecondsTween<float> _strength;
};
