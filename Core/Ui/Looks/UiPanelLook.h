#pragma once
#include <functional>
#include <optional>
#include "Ui/Kernel/UiTypes.h"

// What a panel's drawing gets. Valid during the call.
struct UiPanelView {
    const UiDrawState& state;
    std::optional<Color4> accent; // Empty: the theme's frame colour
};

void DrawPlainPanel(Painter& paint, const UiPanelView& view);

struct UiPanelLook {
    UiInsets padding = UiInsets::All(16.0f); // Added to what the panel's box asks for
    float gap = 8.0f;                        // Between children, when the box asks for none
    std::function<void(Painter&, const UiPanelView&)> draw = DrawPlainPanel;
};
