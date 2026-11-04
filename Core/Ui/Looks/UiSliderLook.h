#pragma once
#include <functional>
#include <string_view>
#include "Ui/Looks/UiControlLook.h"

struct UiSliderView {
    UiControlView control;
    float fraction = 0.0f; // 0..1
    std::string_view valueText;
    FloatRect track{}; // UiSliderTrack
};

void DrawPlainSlider(Painter& paint, const UiSliderView& view);

struct UiSliderLook {
    UiControlMetrics metrics{.controlWidth = 240.0f};
    float valueWidth = 52.0f; // The room at the right of the control for the number
    std::function<void(Painter&, const UiSliderView&)> draw = DrawPlainSlider;
};

// The part of a row that the mouse drags on, as wide as what is drawn.
[[nodiscard]] FloatRect UiSliderTrack(const FloatRect& row, const UiSliderLook& look);
