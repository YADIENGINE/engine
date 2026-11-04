Draw a shared Icon and a new proportional project-local shape.

APIs: Assets/Graphics/DefaultIcons.h, Core/Graphics/Paint/Painter.h.

Call with a positive size in logical canvas units.

For a shared engine icon, add a shape-named Icon, its DrawIcon switch case and its row in OwnColors.
Without a look, an icon draws in its own colors; `{.color = color}` overrides only that color.

<!-- recipe:IconShape -->
```cpp
#include "AgentDocs/Recipes/Code/IconShape.h"
#include "Graphics/DefaultIcons.h"
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
```
<!-- /recipe -->

Pitfalls: Icon size is half its width. Scale all custom offsets and stroke widths with size; name shared icons by shape rather than gameplay meaning.

Concept: `Docs/Graphics/Painter.md#helpers-and-theme`.
