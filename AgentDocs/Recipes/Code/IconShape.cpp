// [recipe:IconShape]
#include "AgentDocs/Recipes/Code/IconShape.h"
#include "Graphics/Icons/Icons.h"
#include "Graphics/Paint/Painter.h"

namespace {
constexpr Float2 BadgeOffsetScale{3.0f, 0.0f};
constexpr Float2 LineStartScale{-0.5f, 0.0f};
constexpr Float2 LineEndScale{0.5f, 0.0f};
constexpr float StrokeScale = 0.12f;
} // namespace

void DrawRecipeIcons(Painter& paint, Float2 center, float size, Color4 color) {
    DrawIcon(paint, Icon::Star, center, size, {.color = color});
    const Float2 badge = center + BadgeOffsetScale * size;
    paint.Circle(badge, size, Outline(StrokeScale * size, color));
    paint.Line(badge + LineStartScale * size, badge + LineEndScale * size,
               {.width = StrokeScale * size, .color = color});
}
// [/recipe]
