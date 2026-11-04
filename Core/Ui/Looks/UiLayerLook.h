#pragma once
#include <functional>
#include "Ui/Kernel/UiTypes.h"

// What a layer's backdrop gets. Valid during the call.
struct UiLayerView {
    const UiDrawState& state;
    float open = 0.0f; // 0..1; the whole layer already fades with it
};

// The theme's overlay colour over the canvas.
void DrawPlainLayerBackdrop(Painter& paint, const UiLayerView& view);

struct UiLayerLook {
    float seconds = 0.15f;
    std::function<void(Painter&, const UiLayerView&)> backdrop = DrawPlainLayerBackdrop;
};
