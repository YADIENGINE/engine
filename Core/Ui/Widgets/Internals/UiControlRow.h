#pragma once
#include <string_view>
#include "Animation/ButtonAnim.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Looks/UiControlLook.h"
#include "Ui/Widgets/UiControlOptions.h"

// The element behind a toggle, a slider and a stepper: a focusable row that takes left and right. 'label' lives until
// the next Begin (Ui::Text).
[[nodiscard]] UiElement OpenUiControlRow(Ui& ui, UiId id, std::string_view label, float textSize,
                                         const UiControlOptions& options, const UiControlMetrics& metrics);
// The row's highlight, press and refusal this frame.
[[nodiscard]] ButtonAnimState AnimateUiControlRow(Ui& ui, UiElement& row, const UiControlMetrics& metrics,
                                                  bool pressed);
