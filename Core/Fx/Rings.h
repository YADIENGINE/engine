#pragma once
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

class Painter;

struct RingStyle {
    float duration = 1.0f;
    float startRadius = 0.0f;
    float endRadius = 0.0f;
    float thickness = 0.0f;
    float fadeSeconds = 0.0f;
};

class RingEffects2D {
public:
    void Emit(Float2 position, Color4 color, const RingStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint) const;

private:
    struct Ring {
        Float2 position;
        Color4 color;
        float life;
        RingStyle style;
    };
    std::vector<Ring> _rings;
};
