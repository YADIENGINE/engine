#pragma once
#include <raylib.h>
#include <vector>

struct RingStyle {
    float duration = 1.0f;
    float startRadius = 0.0f;
    float endRadius = 0.0f;
    float thickness = 0.0f;
    float fadeSeconds = 0.0f;
    int segments = 32;
};

class RingEffects2D {
public:
    void Emit(Vector2 position, Color color, const RingStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw() const;

private:
    struct Ring {
        Vector2 position;
        Color color;
        float life;
        RingStyle style;
    };
    std::vector<Ring> _rings;
};
