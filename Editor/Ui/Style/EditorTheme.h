#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"

enum class EditorGlyph { Play, Pause, Stop, Reset, Build, Cancel, Star, Expand, Restore, More, Clear, Step };
enum class EditorFontRole { Ui, Technical };
enum class EditorTone { Accent, Success, Warning, Error, Information };
void ApplyEditorTheme();
// The single host owns these non-owning roles and clears them before its ImGui context is destroyed.
void RegisterEditorFonts(ImFont* ui, ImFont* technical);
void ClearEditorFonts();
[[nodiscard]] ImFont* EditorFont(EditorFontRole role);
[[nodiscard]] float EditorUiScale();
[[nodiscard]] ImVec4 EditorToneColor(EditorTone tone);
[[nodiscard]] ImVec4 EditorPhaseColor(EditorPhase phase);
[[nodiscard]] ImVec4 EditorLevelColor(EditorLogLevel level);
[[nodiscard]] const char* EditorPhaseLabel(EditorPhase phase);
[[nodiscard]] const char* EditorGlyphText(EditorGlyph glyph);
