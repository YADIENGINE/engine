#pragma once
#include <span>
#include <string_view>
#include "Ui/Kernel/Ui.h"
#include "Ui/Widgets/UiControlOptions.h"

// A row with a label and "< value >". Left, right and clicks on the arrows step it; confirm and a click on the rest of
// the row step it forward. Returns -1, 0 or 1; what a step means is up to the caller, and every step sounds.
[[nodiscard]] int UiStepper(Ui& ui, UiId id, std::string_view label, std::string_view value,
                            const UiControlOptions& options = {});
// The same over a list: moves 'selected', wrapping at both ends. True when it changed.
[[nodiscard]] bool UiStepper(Ui& ui, UiId id, std::string_view label, int& selected,
                             std::span<const std::string_view> choices, const UiControlOptions& options = {});
