#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"

enum class EditorGlyph { Play, Pause, Stop, Reset, Build, Cancel, Star, Expand, Restore };
void ApplyEditorTheme();
// The host keeps the existing technical font first and appends the Inspector label font.
[[nodiscard]] ImFont* EditorInspectorLabelFont();
[[nodiscard]] ImFont* EditorInspectorValueFont();
[[nodiscard]] ImVec4 EditorPhaseColor(EditorPhase phase);
[[nodiscard]] ImVec4 EditorLevelColor(EditorLogLevel level);
[[nodiscard]] const char* EditorPhaseLabel(EditorPhase phase);
[[nodiscard]] const char* EditorGlyphText(EditorGlyph glyph);
