#pragma once
#include "Editor/Ui/Widgets/Properties.h"

// The one widget set for Inspector values (Properties.h). Widgets clamp to the spec's bounds; a value outside them
// cannot be produced here.

// Choice buttons per row before the panel narrows them.
[[nodiscard]] int PropertyChoiceColumns(const PropertySpec& spec);
// Draws the value at the current item width. True on every change, also each step of a drag; ImGui's item state says
// whether the edit is still going on (ImGui::IsAnyItemActive).
[[nodiscard]] bool PropertyWidget(const PropertySpec& spec, PropertyValue& value);
