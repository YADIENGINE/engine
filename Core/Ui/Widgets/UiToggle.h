#pragma once
#include <string_view>
#include "Ui/Kernel/Ui.h"
#include "Ui/Widgets/UiControlOptions.h"

// A row with a label and a switch. Confirm or a click flips it; left turns it off and right on. True when it changed.
[[nodiscard]] bool UiToggle(Ui& ui, UiId id, std::string_view label, bool& value, const UiControlOptions& options = {});
