#pragma once
#include <string_view>
#include "Ui/Kernel/Ui.h"
#include "Ui/Widgets/UiControlOptions.h"

// A row with a label and a slider between 'min' and 'max'. Left and right move it by 'step', and the mouse drags it.
// A 'step' of zero is a twentieth of the range. Values stay on steps. True when it changed.
[[nodiscard]] bool UiSlider(Ui& ui, UiId id, std::string_view label, float& value, float min, float max,
                            float step = 0.0f, const UiControlOptions& options = {});
[[nodiscard]] bool UiSlider(Ui& ui, UiId id, std::string_view label, int& value, int min, int max, int step = 0,
                            const UiControlOptions& options = {});
