#pragma once
#include <string>
#include <vector>
#include "Animation/Curve.h"
#include "Fx/FxText.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

class Painter;

struct FloatingTextStyle {
    float duration = 1.0f;
    Float2 velocity{};
    float fadeSeconds = 0.0f;
    float popSeconds = 0.0f; // zero disables the entrance
    Curve popCurve = Curve::OutBack;
    FxTextStyle text;
};

[[nodiscard]] bool IsValid(const FloatingTextStyle& style);

class FloatingTexts {
public:
    void Emit(Float2 position, const std::string& text, Color4 color, const FloatingTextStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(Painter& paint) const;

private:
    struct Entry {
        Float2 position;
        std::string text;
        Color4 color;
        float life;
        FloatingTextStyle style;
    };
    std::vector<Entry> _entries;
};
