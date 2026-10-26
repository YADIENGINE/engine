#pragma once
#include <imgui.h>
#include "Editor/Api/PreviewParameter.h"

void EditorTooltip(const char* text);
void ContinueEditorToolbar(const char* label, bool separateGroup = false);
void ContinueEditorToolbar(float itemWidth, bool separateGroup = false);
[[nodiscard]] bool EditorWrappedButton(const char* label);
[[nodiscard]] bool EditorWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags);
[[nodiscard]] bool EditorParameterWidget(const PreviewParameter& parameter, PreviewValue& value);
