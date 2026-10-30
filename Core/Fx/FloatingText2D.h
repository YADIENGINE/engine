#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include "Graphics/Draw/TextStyle.h"

class TextRenderer;

struct FloatingTextStyle {
    float duration = 1.0f;
    Vector2 velocity{};
    float fadeSeconds = 0.0f;
    float popSeconds = 0.0f; // zero disables the EaseOutBack entrance
    TextStyle text;
};

class FloatingText2D {
public:
    void Emit(Vector2 position, const std::string& text, Color color, const FloatingTextStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    void Draw(const TextRenderer& textRenderer) const;

private:
    struct Entry {
        Vector2 position;
        std::string text;
        Color color;
        float life;
        FloatingTextStyle style;
    };
    std::vector<Entry> _entries;
};
