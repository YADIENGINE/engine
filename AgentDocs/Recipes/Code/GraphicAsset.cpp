// [recipe:GraphicAsset]
#include "AgentDocs/Recipes/Code/GraphicAsset.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"

void DrawGoblin(Painter& paint, const FloatRect& box, GoblinKind kind, float time) {
    const float size = std::min(box.width, box.height);
    if (size <= 0.0f) return;
    const Float2 center = box.Center() + Float2{0.0f, std::sin(time * 2.0f) * size * 0.03f};
    const Color4 skin = kind == GoblinKind::Forest ? Rgb(0x70c060) : Rgb(0x80c8f0);
    paint.Triangle(center + Float2{-size * 0.38f, -size * 0.24f},
                   center + Float2{-size * 0.12f, -size * 0.16f},
                   center + Float2{-size * 0.16f, size * 0.08f}, Fill(skin));
    paint.Triangle(center + Float2{size * 0.38f, -size * 0.24f},
                   center + Float2{size * 0.16f, size * 0.08f},
                   center + Float2{size * 0.12f, -size * 0.16f}, Fill(skin));
    paint.Ellipse(center, {size * 0.24f, size * 0.30f}, Fill(skin));
    paint.Circle(center + Float2{-size * 0.09f, -size * 0.06f}, size * 0.025f, Fill(Rgb(0x102030)));
    paint.Circle(center + Float2{size * 0.09f, -size * 0.06f}, size * 0.025f, Fill(Rgb(0x102030)));
}
// [/recipe]
