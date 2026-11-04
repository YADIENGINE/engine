#pragma once
#include <functional>
#include <optional>
#include <string_view>
#include "Animation/ButtonAnim.h"
#include "Ui/Kernel/UiTypes.h"

// What a button's drawing gets. Valid during the call.
struct UiButtonView {
    const UiDrawState& state;
    std::string_view label;
    float textSize = 0.0f;
    ButtonAnimState animation;
    std::optional<Color4> accent; // Empty: the theme's accent
};

void DrawPlainButton(Painter& paint, const UiButtonView& view);

struct UiButtonLook {
    float textSize = 24.0f;
    float minWidth = 96.0f;  // For a button that fits its text; UiFill and UiFixed take what they are given
    float minHeight = 44.0f;
    UiInsets padding = UiInsets::Symmetric(20.0f, 8.0f);
    // A light spring on the cursor, a short press and refusal.
    ButtonAnimSettings animation{.highlightStiffness = 400.0f,
                                 .highlightDamping = 32.0f,
                                 .pressSeconds = 0.14f,
                                 .denySeconds = 0.25f};
    std::function<void(Painter&, const UiButtonView&)> draw = DrawPlainButton;
};
