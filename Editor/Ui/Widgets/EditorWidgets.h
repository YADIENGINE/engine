#pragma once
#include "Editor/Api/PreviewParameter.h"

enum class EditorGlyph;

// Editor-only widgets; the shared ones are in ToolUi/ToolWidgets.h.
[[nodiscard]] bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected = false);
[[nodiscard]] bool EditorParameterWidget(const PreviewParameter& parameter, PreviewValue& value);
