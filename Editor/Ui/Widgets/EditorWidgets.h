#pragma once
#include "Editor/Api/SceneParameter.h"

enum class EditorGlyph;

// Editor-only widgets; the shared ones are in ToolUi/ToolWidgets.h.
[[nodiscard]] bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected = false);
// How many columns a choice-button parameter asks for, before a narrow panel reduces them.
[[nodiscard]] int EditorChoiceColumns(const SceneParameter& parameter);
[[nodiscard]] bool EditorParameterWidget(const SceneParameter& parameter, SceneValue& value);
