#include "Ui/Widgets/UiLabel.h"
#include "Graphics/Text/FontFace.h"
#include "Ui/Looks/UiLabelLook.h"

void UiLabel(Ui& ui, std::string_view text, const UiLabelOptions& options) {
    const UiLabelLook& look = ui.Looks().Get<UiLabelLook>();
    const std::string_view copy = ui.Text(text);
    const float size = options.size > 0.0f ? options.size : look.size;
    const FontFace* font = &ui.Font();
    UiTraits traits;
    const bool wrap = options.wrap;
    traits.measure = [font, copy, size, wrap](float maxWidth) {
        if (!wrap || !(maxWidth < UiUnlimited)) return font->Measure(copy, size);
        TextStyle style{.size = size, .maxWidth = maxWidth, .wrap = TextWrap::Word};
        return font->Layout(copy, style).Extent();
    };
    const UiBox box{
        .width = options.width, .height = options.height, .anchor = options.anchor, .offset = options.offset};
    UiElement element = ui.Element(box, traits);
    element.Draw([look = &look, copy, size, color = options.color, align = options.align,
                  wrap](Painter& paint, const UiDrawState& state) {
        look->draw(paint, {.state = state, .text = copy, .size = size, .color = color, .align = align, .wrap = wrap});
    });
}
