Draw a pixel-art character as a sprite: shapes shaded by the kit, parts, frames made once, palette variants and a hit
flash.

APIs: [PixelCanvas.h](../../Core/Graphics/Pixel/PixelCanvas.h), [PixelSprite.h](../../Core/Graphics/Pixel/PixelSprite.h),
[PixelPalette.h](../../Core/Graphics/Pixel/PixelPalette.h), [Painter.h](../../Core/Graphics/Paint/Painter.h).
Concepts: [Pixel art](../../Docs/Graphics/PixelArt.md). Workflow: [graphic-asset skill](../Skills/graphic-asset/SKILL.md).

Start from `python Engine asset new Guard --folder Characters --pixel`, which writes this shape of code with a small
example. Draw volumes as shapes with an `InkRamp` and let the canvas shade them from the light; use `Stamp` only for a
few hand-placed pixels such as eyes. Give each piece a part. One sprite serves every palette: variants change colours,
not pixels. The recipe's include below points to its compiled test copy.

<!-- recipe:PixelSprite-header -->
```cpp
#pragma once
#include "Graphics/Pixel/PixelAnimation.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/Pixel/PixelSpriteSet.h"
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

// Its frames as data: the sprites and the animation that plays them (the game and the editor read the same).
[[nodiscard]] const PixelSpriteSet& GuardSprites();
[[nodiscard]] const PixelAnimation& GuardAnimation();
[[nodiscard]] const PixelSprite& GuardSprite(float time);
[[nodiscard]] PixelPalette GuardPalette(GuardKind kind);
// Centred in `box`, feet on its bottom edge, each sprite pixel a whole number of art pixels.
void DrawGuard(Painter& paint, const FloatRect& box, GuardKind kind, float time, const GuardLook& look = {});
```
<!-- /recipe -->

<!-- recipe:PixelSprite -->
```cpp
#include "AgentDocs/Recipes/Code/PixelSprite.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Pixel/PixelCanvas.h"

namespace {
constexpr int Width = 20;
constexpr int Height = 28;
constexpr int Frames = 2;

// Inks in palette order. Each ramp is one material from dark to light.
enum : Ink { Line = 1, Steel0, Steel1, Steel2, Steel3, Cloth0, Cloth1, Cloth2, Skin, Wood };
constexpr InkRamp Steel{Steel0, Steel1, Steel1, Steel2, Steel3};
constexpr InkRamp Cloth{Cloth0, Cloth0, Cloth1, Cloth2, Cloth2};

PixelSprite PaintGuard(std::size_t frame) {
    PixelCanvas c(Width, Height); // Light from the top left
    const int bob = static_cast<int>(frame);
    {
        auto spear = c.Part(GuardSpear);
        c.FlatLine(16, 2, 16, 26, Wood);
        c.Poly({{15, 0}, {17, 0}, {18, 4}, {14, 4}}, Steel, 1);
    }
    {
        auto body = c.Part(GuardBody);
        c.Limb(7.0f, 20.0f, 7.0f, 27.0f, 2.0f, 1.5f, Cloth, 1); // Legs
        c.Limb(12.0f, 20.0f, 12.0f, 27.0f, 2.0f, 1.5f, Cloth, 1);
        c.Poly({{4, 11 + bob}, {15, 11 + bob}, {14, 21}, {5, 21}}, Steel, 2); // Breastplate
        c.Limb(14.0f, 13.0f + bob, 16.0f, 18.0f, 1.8f, 1.5f, Cloth, 1);       // Arm on the spear
    }
    {
        auto helmet = c.Part(GuardHelmet);
        c.Ellipse(9.5f, 7.0f + bob, 4.5f, 4.5f, Steel, 2);
        c.Stamp(7, 7 + bob, {"9.9", "999"}); // Face under the visor (ink 9 is Skin)
    }
    c.Outline(Line);
    return PixelSprite(c, {Width / 2, Height});
}

const PixelSpriteSet Sprites(Frames, &PaintGuard);
const PixelAnimation Breathing = PixelAnimation::Even(Frames, 2.0f); // Two frames a second
} // namespace

const PixelSpriteSet& GuardSprites() { return Sprites; }

const PixelAnimation& GuardAnimation() { return Breathing; }

const PixelSprite& GuardSprite(float time) { return Sprites[Breathing.SpriteAt(time)]; }

PixelPalette GuardPalette(GuardKind kind) {
    PixelPalette palette{Rgb(0x14121c), Rgb(0x3a4152), Rgb(0x657088), Rgb(0x9aa6b8), Rgb(0xdfe6ee),
                         Rgb(0x4a2230), Rgb(0x7d3245), Rgb(0xb04a5c), Rgb(0xe0a982), Rgb(0x7a5230)};
    if (kind == GuardKind::Royal) { // Same sprite, other cloth
        palette.Set(Cloth0, Rgb(0x1f2a5c));
        palette.Set(Cloth1, Rgb(0x2f4596));
        palette.Set(Cloth2, Rgb(0x4b6fd0));
    }
    return palette;
}

void DrawGuard(Painter& paint, const FloatRect& box, GuardKind kind, float time, const GuardLook& look) {
    // Art pixels per sprite pixel (Painter::ArtPixel units each): as many whole ones as fit.
    const float fit =
        std::min(box.width / static_cast<float>(Width), box.height / static_cast<float>(Height)) / paint.ArtPixel();
    if (fit < 1.0f) return;
    static const PixelPalette Town = GuardPalette(GuardKind::Town);
    static const PixelPalette Royal = GuardPalette(GuardKind::Royal);
    paint.Sprite(GuardSprite(time), kind == GuardKind::Town ? Town : Royal, {box.Center().x, box.Bottom()},
                 {.scale = std::floor(fit), .flash = look.hurtColor, .flashAmount = look.hurt});
}
```
<!-- /recipe -->

Register it with `pixels`, so opening it shows the editor's Pixelator (zoom, inks, parts, frames). It hands over the
same sprites and animation the game plays, so the editor shows exactly the game's frames and durations. Ink and part
names are optional; the Pixelator, lint and `ctl pixels` say them instead of bare numbers:

```cpp
asset.items = {{"Town", "guard soldier"}, {"Royal", "guard soldier blue"}};
asset.usage = "DrawGuard(paint, box, GuardKind::{item}, time);";
asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
    DrawGuard(paint, box, static_cast<GuardKind>(item), time);
};
asset.pixels = [](std::size_t item) {
    auto pixels =
        GraphicPixels::Animated(GuardSprites(), GuardAnimation(), GuardPalette(static_cast<GuardKind>(item)));
    pixels.inkNames = {"Line", "Steel0", "Steel1", "Steel2", "Steel3", "Cloth0", "Cloth1", "Cloth2", "Skin", "Wood"};
    pixels.partNames = {"Helmet", "Body", "Spear"};
    return pixels;
};
```

After drawing or changing it, check it: `python Engine asset lint asset:Graphics/Characters/Guard` (no editor
needed) lists stray pixels, outline doubles and gaps, ramp tones too close to tell apart, unused inks and too many
colours, each with its pixels and frames. Fix warnings in the drawing code; notes (info) are judgement calls.

To look at the pixels yourself, open it and ask for the Pixelator's picture:
`python Engine ctl open asset:Graphics/Characters/Guard#Town`, then
`python Engine ctl pixels --zoom 8` (`--view inks` or `--view parts` shows how it is built, `--frame N` another frame).
The JSON lists the size, pivot, frame count and each ink's colour and pixel count.
