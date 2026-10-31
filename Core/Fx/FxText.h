#pragma once
#include <string_view>
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

class Painter;

// Text an effect draws at a point (floating text, banner, toast): its size, where the point sits on it ('anchor':
// (0, 0) top left .. (1, 1) bottom right of the text), a width it shrinks to fit in and a shadow. The colour comes
// from the effect.
struct FxTextStyle {
    float size = 16.0f;
    Float2 anchor{0.5f, 0.0f};
    float maxWidth = 0.0f; // 0: unlimited
    TextShadow shadow{};
};

// Draws with the game's font (Painter::Font).
void DrawFxText(Painter& paint, std::string_view text, Float2 position, Color4 color, const FxTextStyle& style);
