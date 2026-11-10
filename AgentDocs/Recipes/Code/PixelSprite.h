// [recipe:PixelSprite-header]
#pragma once
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

enum class GuardKind { Town, Royal };

// How the drawing looks on its own; a game changes only what it needs.
struct GuardLook {
    float hurt = 0.0f; // 0..1: the whole sprite flashes toward hurtColor
    Color4 hurtColor{255, 240, 220, 255};
};

// What a guard's pixels belong to, so it can break along them later.
enum GuardPart : PixelPart { GuardHelmet = 1, GuardBody, GuardSpear };

[[nodiscard]] const PixelSprite& GuardSprite(float time);
[[nodiscard]] PixelPalette GuardPalette(GuardKind kind);
// Centred in `box`, feet on its bottom edge, each sprite pixel a whole number of art pixels.
void DrawGuard(Painter& paint, const FloatRect& box, GuardKind kind, float time, const GuardLook& look = {});
// [/recipe]
