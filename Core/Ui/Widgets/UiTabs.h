#pragma once
#include <optional>
#include <span>
#include <string_view>
#include "Ui/Kernel/Ui.h"

struct UiTabsOptions {
    std::optional<Color4> accent{};
    float textSize = 0.0f; // 0: the look's
    bool fill = false;     // Tabs share the width of the row instead of fitting their text
    bool pageKeys = true;  // The previous and next page keys (Q and E) switch tabs wherever the cursor is
};

// A row of tabs: one stop for the cursor, where left and right switch. What each tab shows is up to the caller:
//     if (UiTabs(ui, "pages", Names, _page)) ...
//     switch (_page) { ... }
// True when the selection changed.
[[nodiscard]] bool UiTabs(Ui& ui, UiId id, std::span<const std::string_view> labels, int& selected,
                          const UiTabsOptions& options = {});
