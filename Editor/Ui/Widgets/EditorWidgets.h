#pragma once
#include <cstddef>
#include "Editor/Api/SceneParameter.h"

enum class EditorGlyph;

// Editor-only widgets; the shared ones are in ToolUi/ToolWidgets.h.
[[nodiscard]] bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected = false);
// Search plus a text-only clear button within `width`; `focus` also restores typing after a clear.
void EditorSearchInput(const char* id, const char* hint, char* text, std::size_t capacity, float width, bool& focus);
// How many columns a choice-button parameter asks for, before a narrow panel reduces them.
[[nodiscard]] int EditorChoiceColumns(const SceneParameter& parameter);
[[nodiscard]] bool EditorParameterWidget(const SceneParameter& parameter, SceneValue& value);
