#pragma once
#include <optional>
#include <string_view>
#include "Ui/Kernel/Ui.h"
#include "Ui/Looks/UiButtonLook.h"

struct UiButtonOptions {
    bool enabled = true;
    std::optional<Color4> accent{};
    float textSize = 0.0f; // 0: the look's
    UiSize width{};
    UiSize height{};
    std::optional<UiAnchor> anchor{};
    Float2 offset{};
    const UiButtonLook* look = nullptr; // Borrowed until drawn; empty: the Ui's current look
};

// A button that the mouse and every player's keys can press. 'id' stays the same when the label is translated.
[[nodiscard]] UiAction UiButton(Ui& ui, UiId id, std::string_view label, const UiButtonOptions& options = {});
