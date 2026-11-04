#pragma once
#include <optional>
#include <string_view>
#include "Animation/ButtonAnim.h"
#include "Ui/Kernel/UiTypes.h"

// A row with a label on the left and a control on the right: what a toggle, a slider and a stepper have in common.
struct UiControlMetrics {
    float textSize = 22.0f;
    float minHeight = 44.0f;
    float minWidth = 320.0f; // For a row that fits its content; UiFill and UiFixed take what they are given
    UiInsets padding = UiInsets::Symmetric(14.0f, 6.0f);
    float controlWidth = 220.0f;
    float labelGap = 16.0f;
    ButtonAnimSettings animation{.highlightStiffness = 400.0f,
                                 .highlightDamping = 32.0f,
                                 .pressSeconds = 0.14f,
                                 .denySeconds = 0.25f};
};

// Where the control sits in a row. Input and drawing both use it, so that what is clicked is what is drawn.
[[nodiscard]] FloatRect UiControlArea(const FloatRect& row, const UiControlMetrics& metrics);

// What every control's drawing gets. Valid during the call.
struct UiControlView {
    const UiDrawState& state;
    std::string_view label;
    float textSize = 0.0f;
    ButtonAnimState animation;
    std::optional<Color4> accent; // Empty: the theme's accent
    FloatRect area{};             // UiControlArea
};

// The row's body, frame, cursor and label from the theme's palette.
void DrawPlainControlRow(Painter& paint, const UiControlView& view);
