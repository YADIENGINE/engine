#pragma once
#include <string_view>
#include "Ui/Kernel/Ui.h"

struct UiTooltipOptions {
    bool onFocus = true; // Also for a keyboard cursor resting on it, or keyboard players would never see it
};

// A short explanation that floats by 'anchor' when the mouse or a cursor has rested on it. Call it while the element is
// open. Engine widgets take it as an option: UiButton(ui, "save", "Save", {.tooltip = "Saves to slot one"}).
void UiTooltip(Ui& ui, UiElement& anchor, std::string_view text, const UiTooltipOptions& options = {});
