// [recipe:SpriteLayer]
#include "AgentDocs/Recipes/Code/SpriteLayer.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Surface/Surface.h"

namespace {
constexpr Color4 BackgroundColor = Rgb(0x102030);
constexpr Float2 DecorationCenter{80.0f, 80.0f};
constexpr float DecorationRadius = 48.0f;
constexpr Color4 DecorationColor = Rgb(0x204060);
constexpr float BodyRadius = 32.0f;
constexpr Float2 HeadOffset{24.0f, 0.0f};
constexpr float HeadRadius = 24.0f;
constexpr Color4 SpriteColor = Rgb(0xffc060);
} // namespace

// Owner: Surface background({320.0f, 240.0f}); keep it with the scene/View.
void DrawRecipeLayer(Painter& paint, Surface& background, const FloatRect& destination, float opacity) {
    if (paint.Stale(background)) {
        auto target = paint.Target(background);
        paint.Rect(paint.Bounds(), Fill(BackgroundColor));
        paint.Circle(DecorationCenter, DecorationRadius, Fill(DecorationColor));
    }
    paint.Image(background, destination);
    {
        auto group = paint.Group({.opacity = opacity});
        const Float2 at = destination.Center();
        paint.Circle(at, BodyRadius, Fill(SpriteColor));
        paint.Circle(at + HeadOffset, HeadRadius, Fill(SpriteColor));
    }
}
// [/recipe]
