#pragma once
#include <optional>
#include <string_view>
#include "Graphics/Text/TextStyle.h"
#include "Ui/Kernel/Ui.h"

struct UiLabelOptions {
    float size = 0.0f; // 0: the look's
    std::optional<Color4> color{};
    TextAlign align = TextAlign::Left;
    bool wrap = false; // Otherwise a long text shrinks to fit
    UiSize width{};
    UiSize height{};
    std::optional<UiAnchor> anchor{};
    Float2 offset{};
};

// A text. It wants the room its text needs, and shrinks or wraps when its parent has less.
void UiLabel(Ui& ui, std::string_view text, const UiLabelOptions& options = {});
