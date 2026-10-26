#pragma once
#include <raylib.h>
#include <vector>
#include "Utilities/Rect.h"

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
    Color rim = WHITE;
    int segments = 32;
};

class ShockwaveEffects2D {
public:
    explicit ShockwaveEffects2D(const ShockwaveStyle& style);
    void Emit(Vector2 center, Color color, float reach = 0.0f);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(const Rect& bounds) const;

private:
    struct Wave {
        Vector2 center;
        Color color;
        float reach;
        float age = 0.0f;
    };
    ShockwaveStyle _style;
    std::vector<Wave> _waves;
};
