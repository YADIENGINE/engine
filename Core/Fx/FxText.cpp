#include "Fx/FxText.h"
#include <algorithm>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/FontFace.h"

void DrawFxText(Painter& paint, std::string_view text, Float2 position, Color4 color, const FxTextStyle& style) {
    if (text.empty() || !(style.size > 0.0f) || color.a == 0) return;
    const FontFace& font = paint.Font();
    float size = style.size;
    Float2 extent = font.Measure(text, size);
    if (style.maxWidth > 0.0f && extent.x > style.maxWidth) {
        size *= style.maxWidth / extent.x;
        extent = font.Measure(text, size);
    }
    TextShadow shadow = style.shadow;
    shadow.color = ScaleAlpha(shadow.color, static_cast<float>(color.a) / 255.0f); // Fades with the text
    // Lines align on the anchor's side of the block (centred text stays centred line by line).
    const TextAlign align = style.anchor.x < 0.25f   ? TextAlign::Left
                            : style.anchor.x > 0.75f ? TextAlign::Right
                                                     : TextAlign::Center;
    const float side = align == TextAlign::Left ? 0.0f : align == TextAlign::Right ? 1.0f : 0.5f;
    paint.Text(text, {position.x + extent.x * (side - style.anchor.x), position.y - extent.y * style.anchor.y},
               {.size = size, .color = color, .align = align, .shadow = shadow});
}
