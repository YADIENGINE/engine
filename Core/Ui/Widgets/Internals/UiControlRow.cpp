#include "Ui/Widgets/Internals/UiControlRow.h"
#include <algorithm>
#include "Graphics/Text/FontFace.h"

UiElement OpenUiControlRow(Ui& ui, UiId id, std::string_view label, float textSize, const UiControlOptions& options,
                           const UiControlMetrics& metrics) {
    const FontFace* font = &ui.Font();
    UiBox box{.width = options.width, .height = options.height, .padding = metrics.padding};
    if (box.width.kind == UiSizeKind::Fit) box.width.min = std::max(box.width.min, metrics.minWidth);
    if (box.height.kind == UiSizeKind::Fit) box.height.min = std::max(box.height.min, metrics.minHeight);
    UiTraits traits{.interact = UiInteract::Focus,
                    .capture = UiCapture::Horizontal,
                    .enabled = options.enabled,
                    .sound = UiSound::None};
    const float control = metrics.controlWidth + metrics.labelGap;
    traits.measure = [font, label, textSize, control](float) {
        const Float2 text = font->Measure(label, textSize);
        return Float2{text.x + control, text.y};
    };
    return ui.Element(id, box, traits);
}

ButtonAnimState AnimateUiControlRow(Ui& ui, UiElement& row, const UiControlMetrics& metrics, bool pressed) {
    auto& animation = row.Remember<ButtonAnim>(metrics.animation);
    animation.Update(row.Focused(), ui.Delta());
    if (pressed) animation.Press();
    if (row.Denied()) animation.Deny();
    return animation.State();
}
