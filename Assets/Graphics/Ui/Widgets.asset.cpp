#include "Editor/Api/GraphicAsset.h"
#include "Graphics/Paint/Painter.h"
#include "Ui/Kernel/UiLooks.h"
#include "Ui/Looks/UiButtonLook.h"
#include "Ui/Looks/UiLabelLook.h"
#include "Ui/Looks/UiPanelLook.h"

namespace {
constexpr float WideShare = 0.32f;  // A wide widget's height, of the box
constexpr float LabelShare = 0.11f; // Label size, of the box
enum class Widget { Panel, Label, Button, ButtonWithCursor, DisabledButton };

UiDrawState StateOf(const Painter& paint, const FloatRect& rect) {
    return {.rect = rect, .font = paint.HasFont() ? &paint.Font() : nullptr};
}
FloatRect Wide(const FloatRect& box) {
    return FloatRect::FromCenter(box.Center(), {box.width, box.height * WideShare});
}
void DrawButton(Painter& paint, const FloatRect& box, const UiDrawState& state, float highlight) {
    const UiButtonLook& look = ActiveUiLooks().Get<UiButtonLook>();
    look.draw(paint, {.state = state,
                      .label = "Button",
                      .textSize = box.height * LabelShare,
                      .animation = {.highlight = highlight}});
}

GraphicAsset MakeWidgets() {
    GraphicAsset asset;
    // In Widget's order.
    asset.items = {{"Panel", "ui"},
                   {"Label", "ui label"},
                   {"Button", "ui"},
                   {"Button with cursor", "ui"},
                   {"Disabled button", "ui"}};
    asset.usage = "See AgentDocs/Recipes/UiMenu.md; widgets draw with the Ui's looks.";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float) {
        switch (static_cast<Widget>(item)) {
        case Widget::Panel: ActiveUiLooks().Get<UiPanelLook>().draw(paint, {.state = StateOf(paint, box)}); break;
        case Widget::Label:
            ActiveUiLooks().Get<UiLabelLook>().draw(paint, {.state = StateOf(paint, Wide(box)),
                                                            .text = "Label",
                                                            .size = box.height * LabelShare,
                                                            .align = TextAlign::Center});
            break;
        case Widget::Button: DrawButton(paint, box, StateOf(paint, Wide(box)), 0.0f); break;
        case Widget::ButtonWithCursor: {
            UiDrawState state = StateOf(paint, Wide(box));
            state.focused = true;
            state.focusUsers = 1;
            DrawButton(paint, box, state, 1.0f);
            break;
        }
        case Widget::DisabledButton: {
            UiDrawState state = StateOf(paint, Wide(box));
            state.enabled = false;
            DrawButton(paint, box, state, 0.0f);
            break;
        }
        }
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeWidgets);
} // namespace
