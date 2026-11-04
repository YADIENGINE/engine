#pragma once
#include <optional>
#include "Ui/Kernel/Ui.h"

// A framed box. The look adds its own padding to the box's, and its gap when the box asks for none.
[[nodiscard]] UiElement UiPanel(Ui& ui, UiId id, const UiBox& box = {}, const UiTraits& traits = {},
                                std::optional<Color4> accent = {});
[[nodiscard]] UiElement UiPanel(Ui& ui, const UiBox& box = {}, const UiTraits& traits = {},
                                std::optional<Color4> accent = {});

// Groups that draw nothing: children follow each other from left to right, or from top to bottom.
[[nodiscard]] UiElement UiRow(Ui& ui, UiBox box = {}, const UiTraits& traits = {});
[[nodiscard]] UiElement UiColumn(Ui& ui, UiBox box = {}, const UiTraits& traits = {});
[[nodiscard]] UiElement UiGrid(Ui& ui, int columns, UiBox box = {}, const UiTraits& traits = {});
// Takes the room left in a row or a column, pushing what follows to the far end.
void UiSpacer(Ui& ui, float weight = 1.0f);
