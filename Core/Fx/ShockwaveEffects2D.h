#pragma once
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

// Şok dalgası: verilen erişime (0: alanın köşegeni) kadar genişleyen halka; arkasından 'trailDelay' sonra ikinci,
// ince bir halka gelir.
struct ShockwaveStyle {
    float duration = 1.0f;
    float trailDelay = 0.0f;
    float width = 0.0f;
    float trailWidth = 1.0f;
    float opacity = 1.0f;
    float trailOpacity = 1.0f;
    float rimWidth = 0.0f;
    Color4 rim{255, 255, 255, 255};
};

class ShockwaveEffects2D {
public:
    explicit ShockwaveEffects2D(const ShockwaveStyle& style);
    void Emit(Float2 center, Color4 color, float reach = 0.0f);
    void Update(float deltaSeconds);
    void Clear();
    // 'bounds' gives the full reach (its diagonal) of waves emitted without one.
    void Draw(Painter& paint, const FloatRect& bounds) const;

private:
    struct Wave {
        Float2 center;
        Color4 color;
        float reach;
        float age = 0.0f;
    };
    ShockwaveStyle _style;
    std::vector<Wave> _waves;
};
