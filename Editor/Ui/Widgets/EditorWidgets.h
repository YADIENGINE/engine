#pragma once
#include <imgui.h>
#include "Editor/Api/PreviewParameter.h"

enum class EditorGlyph;

void EditorTooltip(const char* text);
[[nodiscard]] bool EditorSectionHeader(const char* label, bool defaultOpen = true, bool forceOpen = false);
void EditorEmptyState(const char* title, const char* detail);
void EditorStatusBadge(const char* label, ImVec4 color);
[[nodiscard]] bool EditorSegment(const char* label, bool selected);
[[nodiscard]] bool EditorIconButton(const char* id, EditorGlyph glyph, const char* tooltip, bool selected = false);
void ContinueEditorToolbar(const char* label, bool separateGroup = false);
void ContinueEditorToolbar(float itemWidth, bool separateGroup = false);
[[nodiscard]] bool EditorSingleLineButton(const char* label);
[[nodiscard]] bool EditorWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags);
[[nodiscard]] bool EditorParameterWidget(const PreviewParameter& parameter, PreviewValue& value);
