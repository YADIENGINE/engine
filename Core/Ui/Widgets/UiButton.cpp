#include "Ui/Widgets/UiButton.h"
#include <algorithm>
#include "Animation/ButtonAnim.h"
#include "Graphics/Text/FontFace.h"
#include "Ui/Widgets/UiTooltip.h"

UiAction UiButton(Ui& ui, UiId id, std::string_view label, const UiButtonOptions& options) {
    const UiButtonLook& look = options.look ? *options.look : ui.Looks().Get<UiButtonLook>();
    const std::string_view text = ui.Text(label);
    const float textSize = options.textSize > 0.0f ? options.textSize : look.textSize;
    const FontFace* font = &ui.Font();

    UiBox box{.width = options.width,
              .height = options.height,
              .padding = look.padding,
              .anchor = options.anchor,
              .offset = options.offset};
    // A look's minimum is for a button that sizes itself; room given by the screen is not overruled.
    if (box.width.kind == UiSizeKind::Fit) box.width.min = std::max(box.width.min, look.minWidth);
    if (box.height.kind == UiSizeKind::Fit) box.height.min = std::max(box.height.min, look.minHeight);
    UiTraits traits{.interact = UiInteract::Focus, .enabled = options.enabled};
    traits.measure = [font, text, textSize](float) { return font->Measure(text, textSize); };
    UiElement element = ui.Element(id, box, traits);

    auto& animation = element.Remember<ButtonAnim>(look.animation);
    animation.Update(element.Focused(), ui.Delta());
    const UiAction action = element.Activated();
    if (action) animation.Press();
    if (element.Denied()) animation.Deny();

    element.Draw([look = &look, text, textSize, state = animation.State(),
                  accent = options.accent](Painter& paint, const UiDrawState& drawn) {
        look->draw(paint, {.state = drawn, .label = text, .textSize = textSize, .animation = state, .accent = accent});
    });
    if (!options.tooltip.empty()) UiTooltip(ui, element, options.tooltip);
    return action;
}
