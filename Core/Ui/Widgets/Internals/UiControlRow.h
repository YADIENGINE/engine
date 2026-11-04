#pragma once
#include <string_view>
#include "Animation/ButtonAnim.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Looks/UiControlLook.h"
#include "Ui/Widgets/UiControlOptions.h"

// The element behind a toggle, a slider and a stepper: a focusable row that takes left and right. 'label' lives until
// the next Begin (Ui::Text). A press on it is silent: what sounds is the value changing, which the widget reports, so a
// slider's drag does not end in a confirm and a press and a change never play together.
[[nodiscard]] UiElement OpenUiControlRow(Ui& ui, UiId id, std::string_view label, float textSize,
                                         const UiControlOptions& options, const UiControlMetrics& metrics);
// The row's highlight, press and refusal this frame.
[[nodiscard]] ButtonAnimState AnimateUiControlRow(Ui& ui, UiElement& row, const UiControlMetrics& metrics,
                                                  bool pressed);
