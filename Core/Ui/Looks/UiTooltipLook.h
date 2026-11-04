#pragma once
#include <functional>
#include "Ui/Kernel/UiTypes.h"

void DrawPlainTooltip(Painter& paint, const UiDrawState& state);

struct UiTooltipLook {
    float delay = 0.5f;
    float maxWidth = 360.0f;
    float textSize = 18.0f;
    UiInsets padding = UiInsets::Symmetric(10.0f, 6.0f);
    float distance = 8.0f; // From the element it explains
    float fadeSeconds = 0.12f;
    std::function<void(Painter&, const UiDrawState&)> draw = DrawPlainTooltip;
};
