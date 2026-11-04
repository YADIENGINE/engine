#include "Ui/Looks/UiLabelLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

TextStyle UiLabelTextStyle(const UiLabelView& view, Color4 color) {
    TextStyle style{.size = view.size, .color = color, .align = view.align, .maxWidth = view.state.rect.width};
    if (view.wrap) {
        style.wrap = TextWrap::Word;
    } else {
        style.fit = TextFit::Shrink;
    }
    return style;
}

void DrawPlainLabel(Painter& paint, const UiLabelView& view) {
    if (!view.state.font) return;
    const Color4 color = view.color.value_or(CurrentTheme().palette.text);
    paint.TextBox(*view.state.font, view.text, view.state.rect, UiLabelTextStyle(view, color));
}
