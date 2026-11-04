#pragma once
#include <optional>
#include <string_view>
#include "Ui/Layout/UiLayout.h"
#include "Utilities/Color4.h"

// What a toggle, a slider and a stepper have in common.
struct UiControlOptions {
    bool enabled = true;
    std::optional<Color4> accent{};
    float textSize = 0.0f; // 0: the look's
    UiSize width = UiFill();
    UiSize height{};
    std::string_view tooltip{};
};
