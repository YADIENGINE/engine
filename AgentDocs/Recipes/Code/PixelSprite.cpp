// [recipe:PixelSprite]
#include "AgentDocs/Recipes/Code/PixelSprite.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelSpriteSet.h"

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
} // namespace

const PixelSprite& GuardSprite(float time) {
    return Sprites[static_cast<std::size_t>(std::max(0.0f, std::floor(time * 2.0f))) % Frames];
}

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
    const float fit = std::min(box.width / static_cast<float>(Width), box.height / static_cast<float>(Height));
    if (fit < 1.0f) return;
    static const PixelPalette Town = GuardPalette(GuardKind::Town);
    static const PixelPalette Royal = GuardPalette(GuardKind::Royal);
    paint.Sprite(GuardSprite(time), kind == GuardKind::Town ? Town : Royal, {box.Center().x, box.Bottom()},
                 {.scale = std::floor(fit), .flash = look.hurtColor, .flashAmount = look.hurt});
}
// [/recipe]
