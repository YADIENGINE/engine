#include "Editor/Ui/Style/EditorTheme.h"
#include <extras/IconsFontAwesome6.h>
#include <cmath>
#include <numbers>
#include "ToolUi/ToolTheme.h"

float EditorAttentionPulse() {
    constexpr double PeriodSeconds = 1.4;
    const float pulse =
        static_cast<float>(0.5 + 0.5 * std::sin(ImGui::GetTime() * 2 * std::numbers::pi / PeriodSeconds));
    return 0.12f + (0.48f - 0.12f) * pulse;
}

const char* EditorSessionStateIcon(SessionState state) {
    switch (state) {
    case SessionState::Working: return ICON_FA_CLOCK;
    case SessionState::Waiting: return ICON_FA_CIRCLE_QUESTION;
    case SessionState::Done   : return ICON_FA_CHECK;
    case SessionState::Idle   :
    case SessionState::Exited : return "";
    }
    return "";
}
ImVec4 EditorSessionStateColor(SessionState state) {
    switch (state) {
    case SessionState::Working: return ToolToneColor(ToolTone::Accent);
    case SessionState::Waiting: {
        auto color = ToolToneColor(ToolTone::Warning);
        color.w *= 0.52f + EditorAttentionPulse();
        return color;
    }
    case SessionState::Done  : return ToolToneColor(ToolTone::Success);
    case SessionState::Exited: return ToolPalette::Disabled;
    case SessionState::Idle  : return ToolPalette::Secondary;
    }
    return ToolPalette::Secondary;
}

ImVec4 EditorPhaseColor(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::Ready         : return ToolToneColor(ToolTone::Success);
    case EditorPhase::Error         : return ToolToneColor(ToolTone::Error);
    case EditorPhase::Paused        : return ToolToneColor(ToolTone::Warning);
    case EditorPhase::ReadingCatalog:
    case EditorPhase::Starting      :
    case EditorPhase::Building      : return ToolToneColor(ToolTone::Information);
    case EditorPhase::Stopped       : return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
    return {};
}
ImVec4 EditorLevelColor(EditorLogLevel level) {
    if (level == EditorLogLevel::Error) return EditorPhaseColor(EditorPhase::Error);
    if (level == EditorLogLevel::Warning) return EditorPhaseColor(EditorPhase::Paused);
    return ToolToneColor(ToolTone::Information);
}
const char* EditorPhaseLabel(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::ReadingCatalog: return "Reading catalog";
    case EditorPhase::Starting      : return "Starting";
    case EditorPhase::Ready         : return "Ready";
    case EditorPhase::Paused        : return "Paused";
    case EditorPhase::Building      : return "Building";
    case EditorPhase::Stopped       : return "Stopped";
    case EditorPhase::Error         : return "Error";
    }
    return "";
}
const char* EditorGlyphText(EditorGlyph glyph) {
    switch (glyph) {
    case EditorGlyph::Play   : return ICON_FA_PLAY;
    case EditorGlyph::Pause  : return ICON_FA_PAUSE;
    case EditorGlyph::Stop   : return ICON_FA_STOP;
    case EditorGlyph::Reset  : return ICON_FA_ROTATE_LEFT;
    case EditorGlyph::Build  : return ICON_FA_HAMMER;
    case EditorGlyph::Cancel : return ICON_FA_XMARK;
    case EditorGlyph::Star   : return ICON_FA_STAR;
    case EditorGlyph::Expand : return ICON_FA_EXPAND;
    case EditorGlyph::Restore: return ICON_FA_COMPRESS;
    case EditorGlyph::More   : return ICON_FA_ELLIPSIS;
    case EditorGlyph::Clear  : return ICON_FA_TRASH_CAN;
    case EditorGlyph::Step   : return ICON_FA_FORWARD_STEP;
    case EditorGlyph::Mark   : return ICON_FA_CROP_SIMPLE;
    }
    return "";
}
