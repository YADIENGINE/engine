#pragma once
#include <imgui.h>
#include "ToolUi/ToolTheme.h"

// Shared widgets of the engine's tools; they read the tool theme (ToolUi/ToolTheme.h) and shorten or wrap text
// at narrow widths, keeping the full text in a tooltip.
void ToolTooltip(const char* text);
// Bounded desktop panes with explicit surface roles. Always pair with EndToolPanel, even when clipped.
[[nodiscard]] bool BeginToolPanel(const char* id, ImVec2 size, ToolSurface surface, ImGuiChildFlags flags = 0,
                                  ImGuiWindowFlags windowFlags = 0, float padding = ToolMetrics::Padding);
void EndToolPanel();
[[nodiscard]] bool ToolSectionHeader(const char* label, bool defaultOpen = true, bool forceOpen = false);
void ToolEmptyState(const char* title, const char* detail);
void ToolStatusBadge(const char* label, ImVec4 color);
// Medium-weight labels with the same sizing, hover, pressed, disabled and keyboard focus behavior.
[[nodiscard]] bool ToolButton(const char* label, ImVec2 size = {}, bool primary = false);
[[nodiscard]] bool ToolSegment(const char* label, bool selected);
// `icon` is a Font Awesome glyph (ICON_FA_* from <extras/IconsFontAwesome6.h>), merged into the Ui font.
[[nodiscard]] bool ToolIconButton(const char* id, const char* icon, const char* tooltip, bool selected = false);
// Places the next toolbar item on the same row when it fits, otherwise on a new row.
void ContinueToolToolbar(const char* label, bool separateGroup = false);
void ContinueToolToolbar(float itemWidth, bool separateGroup = false);
[[nodiscard]] bool ToolSingleLineButton(const char* label, bool primary = false);
[[nodiscard]] bool ToolWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags);
