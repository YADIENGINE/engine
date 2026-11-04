#pragma once
#include <functional>
#include "Ui/Kernel/UiTypes.h"

// What a scroll bar's drawing gets. Valid during the call.
struct UiScrollView {
    const UiDrawState& state; // state.rect is the track
    FloatRect thumb{};
    bool dragging = false;
};

void DrawPlainScrollBar(Painter& paint, const UiScrollView& view);

struct UiScrollLook {
    float barWidth = 8.0f;
    float barGap = 6.0f;      // Between the content and the bar
    float minThumb = 24.0f;
    float wheelStep = 60.0f;  // Units per wheel notch
    float margin = 8.0f;      // Kept around a cursor brought into view
    float speed = 18.0f;      // How quickly it reaches where it is going
    std::function<void(Painter&, const UiScrollView&)> draw = DrawPlainScrollBar;
};
