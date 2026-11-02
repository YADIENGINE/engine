#pragma once
#include <imgui.h>
#include <string>
#include "ToolUi/ToolTheme.h"

// Shared widgets of the engine's tools; they read the tool theme (ToolUi/ToolTheme.h) and shorten or wrap text
// at narrow widths, keeping the full text in a tooltip.
void ToolTooltip(const char* text);
// `text` shortened to `width` in the current font: the end replaced by "...", or for a path the middle, so its last
// folder stays readable. Unchanged when it fits; empty when not even the ellipsis fits.
[[nodiscard]] std::string ToolClipText(const char* text, float width);
[[nodiscard]] std::string ToolClipMiddle(const char* text, float width);
// Bounded desktop panes with explicit surface roles. Always pair with EndToolPanel, even when clipped.
[[nodiscard]] bool BeginToolPanel(const char* id, ImVec2 size, ToolSurface surface, ImGuiChildFlags flags = 0,
                                  ImGuiWindowFlags windowFlags = 0, float padding = ToolMetrics::Padding);
void EndToolPanel();
[[nodiscard]] bool ToolSectionHeader(const char* label, bool defaultOpen = true, bool forceOpen = false);
void ToolEmptyState(const char* title, const char* detail);
void ToolStatusBadge(const char* label, ImVec4 color);
// Medium-weight labels with the same sizing, hover, pressed, disabled and keyboard focus behavior.
[[nodiscard]] bool ToolButton(const char* label, ImVec2 size = {}, bool primary = false);
// Text with an amber underline when selected. `attention`: a fill behind it for "look here" states; alpha 0 for none.
[[nodiscard]] bool ToolSegment(const char* label, bool selected, ImVec4 attention = {0, 0, 0, 0});
// `icon` is a Font Awesome glyph (ICON_FA_* from <extras/IconsFontAwesome6.h>), merged into the Ui font.
[[nodiscard]] bool ToolIconButton(const char* id, const char* icon, const char* tooltip, bool selected = false);
// A switch with its label on the right ("##" hides the label). Returns true when it flipped.
[[nodiscard]] bool ToolToggle(const char* label, bool& value);
[[nodiscard]] float ToolToggleWidth(const char* label);
// BeginCombo with a thin chevron; pair with ImGui::EndCombo when it returns true.
[[nodiscard]] bool ToolBeginCombo(const char* label, const char* preview, ImGuiComboFlags flags = 0);
// Places the next toolbar item on the same row when it fits, otherwise on a new row.
void ContinueToolToolbar(const char* label, bool separateGroup = false);
void ContinueToolToolbar(float itemWidth, bool separateGroup = false);
[[nodiscard]] bool ToolSingleLineButton(const char* label, bool primary = false);
// `reservedRight`: width kept free at the row's end for a control placed over it.
[[nodiscard]] bool ToolWrappedSelectable(const char* label, bool selected, ImGuiSelectableFlags flags,
                                         float reservedRight = 0);
