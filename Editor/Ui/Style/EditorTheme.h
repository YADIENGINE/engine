#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"

// Editor-only presentation on top of the shared tool theme (ToolUi/ToolTheme.h).
struct EditorToolbarMetrics {
    static constexpr float ControlHeight = 30;
    static constexpr float Gap = 6;
    static constexpr float TransportGap = 2; // Between the buttons of the centered playback group
    static constexpr float GroupGap = 16;    // Between the bar's three zones
    static constexpr float ButtonPadding = 10;
    static constexpr float PaddingX = 12;
    static constexpr float PaddingY = 7;
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
// The hue only the agent uses: amber stays the primary button's, green and red stay results.
[[nodiscard]] ImVec4 EditorAgentColor();
[[nodiscard]] ImVec4 EditorTintSurface(ImVec4 base, ImVec4 tone, float amount);
