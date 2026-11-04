#pragma once
#include <vector>
#include "Animation/Curve.h"
#include "Fx/FxQuality.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// Shockwave: a ring expanding to the given reach (0: area diagonal), followed by a second, thin ring after
// 'trailDelay'.
struct ShockwaveStyle {
    float duration = 1.0f;
    float trailDelay = 0.0f;
    float width = 0.0f;
    float trailWidth = 1.0f;
    float opacity = 1.0f;
    float trailOpacity = 1.0f;
    float rimWidth = 0.0f;
    Color4 rim{255, 255, 255, 255};
    Curve curve = Curve::OutCubic; // How the radius travels
};

[[nodiscard]] bool IsValid(const ShockwaveStyle& style);

class Shockwaves {
public:
    // Flashes off: Emit does nothing, since a wave lights the screen as it crosses.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // An invalid style or reach throws invalid_argument.
    void Emit(Float2 center, Color4 color, const ShockwaveStyle& style, float reach = 0.0f);
    void Update(float deltaSeconds);
    void Clear();
    // 'bounds' gives the full reach (its diagonal) of waves emitted without one.
    void Draw(Painter& paint, const FloatRect& bounds) const;

private:
    struct Wave {
        Float2 center;
        Color4 color;
        float reach;
        ShockwaveStyle style;
        float age = 0.0f;
    };
    FxQuality _quality;
    std::vector<Wave> _waves;
};
