#pragma once
#include <imgui.h>

// The shared look of the engine's tools (editor, launcher): one dark theme, two font roles and semantic tones.
enum class ToolFontRole { Ui, Technical };
// Accent: selection / focus; Success, Warning, Error, Information: state and message colors.
enum class ToolTone { Accent, Success, Warning, Error, Information };
void ApplyToolTheme();
// The single ToolUiContext owns these non-owning roles and clears them before its ImGui context is destroyed.
void RegisterToolFonts(ImFont* ui, ImFont* technical);
void ClearToolFonts();
[[nodiscard]] ImFont* ToolFont(ToolFontRole role);
[[nodiscard]] float ToolUiScale();
[[nodiscard]] ImVec4 ToolToneColor(ToolTone tone);
