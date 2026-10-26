#pragma once
#include <raylib.h>

enum class TextAlignment { Left, Center };

struct MultilineTextStyle {
    TextAlignment alignment = TextAlignment::Left;
    int lineSpacing = 0; // added to the layout's nominal font size
};

struct TextStyle {
    int fontSize = 16;
    Vector2 anchor{0.5f, 0.0f}; // normalized anchor within the nominal text bounds
    int maxWidth = 0;
    Color shadow = BLANK;
    Vector2 shadowOffset{};
};
