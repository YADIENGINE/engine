#pragma once
#include <functional>
#include <string_view>
#include "Ui/Looks/UiControlLook.h"

struct UiStepperView {
    UiControlView control;
    std::string_view valueText;
};

void DrawPlainStepper(Painter& paint, const UiStepperView& view);

struct UiStepperLook {
    UiControlMetrics metrics{.controlWidth = 220.0f};
    std::function<void(Painter&, const UiStepperView&)> draw = DrawPlainStepper;
};
