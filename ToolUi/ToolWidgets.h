#pragma once
#include <imgui.h>

// Shared widgets of the engine's tools; they read the tool theme (ToolUi/ToolTheme.h) and shorten or wrap text
// at narrow widths, keeping the full text in a tooltip.
void ToolTooltip(const char* text);
[[nodiscard]] bool ToolSectionHeader(const char* label, bool defaultOpen = true, bool forceOpen = false);
void ToolEmptyState(const char* title, const char* detail);
void ToolStatusBadge(const char* label, ImVec4 color);
[[nodiscard]] bool ToolSegment(const char* label, bool selected);
// `icon` is a Font Awesome glyph (ICON_FA_* from <extras/IconsFontAwesome6.h>), merged into the Ui font.
[[nodiscard]] bool ToolIconButton(const char* id, const char* icon, const char* tooltip, bool selected = false);
// Places the next toolbar item on the same row when it fits, otherwise on a new row.
void ContinueToolToolbar(const char* label, bool separateGroup = false);
void ContinueToolToolbar(float itemWidth, bool separateGroup = false);
[[nodiscard]] bool ToolSingleLineButton(const char* label);
[[nodiscard]] bool ToolWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags);
