#include "Editor/Ui/Style/EditorTheme.h"
#include <extras/IconsFontAwesome6.h>

ImFont* EditorInspectorLabelFont() { return ImGui::GetIO().Fonts->Fonts.back(); }
ImFont* EditorInspectorValueFont() { return ImGui::GetIO().Fonts->Fonts.front(); }

void ApplyEditorTheme() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {12, 12};
    style.FramePadding = {10, 6};
    style.ItemSpacing = {8, 8};
    style.WindowRounding = 6;
    style.ChildRounding = 6;
    style.FrameRounding = 5;
    style.PopupRounding = 6;
    style.GrabRounding = 4;
    style.TabRounding = 4;
    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = {0.87f, 0.91f, 0.96f, 1};
    colors[ImGuiCol_TextDisabled] = {0.49f, 0.56f, 0.65f, 1};
    colors[ImGuiCol_WindowBg] = {0.075f, 0.09f, 0.118f, 1};
    colors[ImGuiCol_ChildBg] = {0.055f, 0.067f, 0.09f, 1};
    colors[ImGuiCol_PopupBg] = {0.09f, 0.11f, 0.145f, 1};
    colors[ImGuiCol_MenuBarBg] = {0.085f, 0.105f, 0.14f, 1};
    colors[ImGuiCol_Border] = {0.19f, 0.23f, 0.29f, 0.8f};
    colors[ImGuiCol_FrameBg] = {0.12f, 0.15f, 0.20f, 1};
    colors[ImGuiCol_FrameBgHovered] = {0.16f, 0.23f, 0.30f, 1};
    colors[ImGuiCol_FrameBgActive] = {0.16f, 0.30f, 0.37f, 1};
    colors[ImGuiCol_Button] = {0.14f, 0.20f, 0.26f, 1};
    colors[ImGuiCol_ButtonHovered] = {0.17f, 0.33f, 0.41f, 1};
    colors[ImGuiCol_ButtonActive] = {0.13f, 0.42f, 0.48f, 1};
    colors[ImGuiCol_Header] = {0.12f, 0.28f, 0.34f, 1};
    colors[ImGuiCol_HeaderHovered] = {0.15f, 0.35f, 0.42f, 1};
    colors[ImGuiCol_HeaderActive] = {0.14f, 0.40f, 0.46f, 1};
    colors[ImGuiCol_CheckMark] = {0.33f, 0.85f, 0.80f, 1};
    colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TabSelected] = colors[ImGuiCol_Header];
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_DockingPreview] = {0.33f, 0.85f, 0.80f, 0.4f};
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
}
ImVec4 EditorPhaseColor(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::Ready         : return {0.33f, 0.85f, 0.80f, 1};
    case EditorPhase::Error         : return {1, 0.42f, 0.42f, 1};
    case EditorPhase::Paused        : return {1, 0.78f, 0.35f, 1};
    case EditorPhase::ReadingCatalog:
    case EditorPhase::Starting      :
    case EditorPhase::Stopping      :
    case EditorPhase::Building      : return {0.50f, 0.73f, 1, 1};
    case EditorPhase::Stopped       : return {0.49f, 0.56f, 0.65f, 1};
    }
    return {};
}
ImVec4 EditorLevelColor(EditorLogLevel level) {
    if (level == EditorLogLevel::Error) return EditorPhaseColor(EditorPhase::Error);
    if (level == EditorLogLevel::Warning) return EditorPhaseColor(EditorPhase::Paused);
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}
const char* EditorPhaseLabel(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::ReadingCatalog: return "Reading catalog";
    case EditorPhase::Starting      : return "Starting";
    case EditorPhase::Ready         : return "Ready";
    case EditorPhase::Paused        : return "Paused";
    case EditorPhase::Stopping      : return "Stopping";
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
    }
    return "";
}
