#pragma once
#include <functional>
#include "Ui/Looks/UiControlLook.h"

struct UiToggleView {
    UiControlView control;
    float on = 0.0f; // 0: off; 1: on; between while it moves
};

void DrawPlainToggle(Painter& paint, const UiToggleView& view);

struct UiToggleLook {
    UiControlMetrics metrics{.controlWidth = 64.0f};
    std::function<void(Painter&, const UiToggleView&)> draw = DrawPlainToggle;
};
