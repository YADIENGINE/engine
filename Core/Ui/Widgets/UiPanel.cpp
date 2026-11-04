#include "Ui/Widgets/UiPanel.h"
#include "Ui/Looks/UiPanelLook.h"

namespace {
[[nodiscard]] UiBox WithLook(UiBox box, const UiPanelLook& look) {
    box.padding.left += look.padding.left;
    box.padding.top += look.padding.top;
    box.padding.right += look.padding.right;
    box.padding.bottom += look.padding.bottom;
    if (box.gap == 0.0f) box.gap = look.gap;
    return box;
}
void DrawWithLook(UiElement& element, const UiPanelLook& look, std::optional<Color4> accent) {
    element.Draw([look = &look, accent](Painter& paint, const UiDrawState& state) {
        look->draw(paint, {.state = state, .accent = accent});
    });
}
} // namespace

UiElement UiPanel(Ui& ui, UiId id, const UiBox& box, const UiTraits& traits, std::optional<Color4> accent) {
    const UiPanelLook& look = ui.Looks().Get<UiPanelLook>();
    UiElement element = ui.Element(id, WithLook(box, look), traits);
    DrawWithLook(element, look, accent);
    return element;
}
UiElement UiPanel(Ui& ui, const UiBox& box, const UiTraits& traits, std::optional<Color4> accent) {
    const UiPanelLook& look = ui.Looks().Get<UiPanelLook>();
    UiElement element = ui.Element(WithLook(box, look), traits);
    DrawWithLook(element, look, accent);
    return element;
}
UiElement UiRow(Ui& ui, UiBox box, const UiTraits& traits) {
    box.direction = UiDirection::Row;
    return ui.Element(box, traits);
}
UiElement UiColumn(Ui& ui, UiBox box, const UiTraits& traits) {
    box.direction = UiDirection::Column;
    return ui.Element(box, traits);
}
void UiSpacer(Ui& ui, float weight) {
    const UiElement spacer = ui.Element({.width = UiFill(weight), .height = UiFill(weight)});
}
