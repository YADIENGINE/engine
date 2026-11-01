#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"

// Editor-only presentation on top of the shared tool theme (ToolUi/ToolTheme.h).
struct EditorToolbarMetrics {
    static constexpr float ControlHeight = 36;
    static constexpr float Radius = 6;
    static constexpr float Gap = 6;
    static constexpr float ButtonPadding = 11;
    static constexpr float PaddingX = 10;
    static constexpr float PaddingY = 8;
};

enum class EditorGlyph { Play, Pause, Stop, Reset, Build, Cancel, Star, Expand, Restore, More, Clear, Step, Mark };
[[nodiscard]] ImVec4 EditorPhaseColor(EditorPhase phase);
[[nodiscard]] ImVec4 EditorLevelColor(EditorLogLevel level);
[[nodiscard]] const char* EditorPhaseLabel(EditorPhase phase);
[[nodiscard]] const char* EditorGlyphText(EditorGlyph glyph);
// Console rows and the status bar use the same session indicators; Idle and Exited have no glyph.
[[nodiscard]] const char* EditorSessionStateIcon(SessionState state);
[[nodiscard]] ImVec4 EditorSessionStateColor(SessionState state);
// Shared notification rhythm for unread Output and agent attention (0.12 .. 0.48).
[[nodiscard]] float EditorAttentionPulse();
