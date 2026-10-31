#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"

// Editor-only presentation on top of the shared tool theme (ToolUi/ToolTheme.h).
struct EditorToolbarMetrics {
    static constexpr float ControlHeight = 36;
    static constexpr float Radius = 6;
    static constexpr float Gap = 6;
    static constexpr float ButtonPadding = 11;
    static constexpr float PaddingX = 10;
    static constexpr float PaddingY = 8;
};

enum class EditorGlyph { Play, Pause, Stop, Reset, Build, Cancel, Star, Expand, Restore, More, Clear, Step };
[[nodiscard]] ImVec4 EditorPhaseColor(EditorPhase phase);
[[nodiscard]] ImVec4 EditorLevelColor(EditorLogLevel level);
[[nodiscard]] const char* EditorPhaseLabel(EditorPhase phase);
[[nodiscard]] const char* EditorGlyphText(EditorGlyph glyph);
