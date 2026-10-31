#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"

// Editor-only presentation on top of the shared tool theme (ToolUi/ToolTheme.h).
enum class EditorGlyph { Play, Pause, Stop, Reset, Build, Cancel, Star, Expand, Restore, More, Clear, Step };
[[nodiscard]] ImVec4 EditorPhaseColor(EditorPhase phase);
[[nodiscard]] ImVec4 EditorLevelColor(EditorLogLevel level);
[[nodiscard]] const char* EditorPhaseLabel(EditorPhase phase);
[[nodiscard]] const char* EditorGlyphText(EditorGlyph glyph);
