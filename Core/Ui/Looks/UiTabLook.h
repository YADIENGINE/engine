#pragma once
#include <functional>
#include <optional>
#include <string_view>
#include "Animation/ButtonAnim.h"
#include "Ui/Kernel/UiTypes.h"

// What one tab's drawing gets. Valid during the call.
struct UiTabView {
    const UiDrawState& state;
    std::string_view label;
    float textSize = 0.0f;
    bool selected = false;
    bool cursor = false; // The tab row has a player's cursor, and this is its selected tab
    ButtonAnimState animation;
    std::optional<Color4> accent; // Empty: the theme's accent
};

void DrawPlainTab(Painter& paint, const UiTabView& view);

struct UiTabLook {
    float textSize = 22.0f;
    float minWidth = 72.0f;
    float minHeight = 40.0f;
    UiInsets padding = UiInsets::Symmetric(16.0f, 6.0f);
    float gap = 4.0f;
    ButtonAnimSettings animation{.highlightStiffness = 400.0f, .highlightDamping = 32.0f, .pressSeconds = 0.14f};
    std::function<void(Painter&, const UiTabView&)> draw = DrawPlainTab;
};
