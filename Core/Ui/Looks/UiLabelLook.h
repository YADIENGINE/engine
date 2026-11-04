#pragma once
#include <functional>
#include <optional>
#include <string_view>
#include "Graphics/Text/TextStyle.h"
#include "Ui/Kernel/UiTypes.h"

// What a label's drawing gets. Valid during the call.
struct UiLabelView {
    const UiDrawState& state;
    std::string_view text;
    float size = 0.0f;
    std::optional<Color4> color; // Empty: the look's own
    TextAlign align = TextAlign::Left;
    bool wrap = false;
};

void DrawPlainLabel(Painter& paint, const UiLabelView& view);

struct UiLabelLook {
    float size = 24.0f;
    std::function<void(Painter&, const UiLabelView&)> draw = DrawPlainLabel;
};

// The text style of a label, for looks and for measuring it.
[[nodiscard]] TextStyle UiLabelTextStyle(const UiLabelView& view, Color4 color);
