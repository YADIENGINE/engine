#include "Editor/Ui/Style/EditorTheme.h"
#include <extras/IconsFontAwesome6.h>
#include <algorithm>

namespace {
constexpr float BaseFontSize = 14.0f;
ImGuiContext* FontsContext = nullptr;
ImFont* UiFont = nullptr;
ImFont* TechnicalFont = nullptr;
} // namespace

void RegisterEditorFonts(ImFont* ui, ImFont* technical) {
    FontsContext = ImGui::GetCurrentContext();
    UiFont = ui;
    TechnicalFont = technical;
    ImGui::GetIO().FontDefault = ui;
}
void ClearEditorFonts() {
    if (FontsContext != ImGui::GetCurrentContext()) return;
    FontsContext = nullptr;
    UiFont = nullptr;
    TechnicalFont = nullptr;
}
ImFont* EditorFont(EditorFontRole role) {
    if (FontsContext == ImGui::GetCurrentContext()) {
        if (auto* font = role == EditorFontRole::Ui ? UiFont : TechnicalFont) return font;
    }
    // Small UI fixtures and other ImGui contexts do not register editor fonts.
    return ImGui::GetIO().FontDefault ? ImGui::GetIO().FontDefault : ImGui::GetFont();
}
float EditorUiScale() { return std::max(0.5f, ImGui::GetFontSize() / BaseFontSize); }

void ApplyEditorTheme() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {12, 12};
    style.FramePadding = {8, 7};
    style.ItemSpacing = {8, 4};
    style.ItemInnerSpacing = {4, 4};
    style.CellPadding = {8, 4};
    style.WindowRounding = 6;
    style.ChildRounding = 4;
    style.FrameRounding = 4;
    style.PopupRounding = 6;
    style.GrabRounding = 4;
    style.TabRounding = 4;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.ScrollbarSize = 12;
    style.ScrollbarRounding = 6;
    style.GrabMinSize = 8;
    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = {230 / 255.0f, 233 / 255.0f, 239 / 255.0f, 1};
    colors[ImGuiCol_TextDisabled] = {160 / 255.0f, 168 / 255.0f, 183 / 255.0f, 1};
    colors[ImGuiCol_WindowBg] = {35 / 255.0f, 38 / 255.0f, 43 / 255.0f, 1};
    colors[ImGuiCol_ChildBg] = {28 / 255.0f, 30 / 255.0f, 34 / 255.0f, 1};
    colors[ImGuiCol_PopupBg] = {0.14f, 0.145f, 0.165f, 1};
    colors[ImGuiCol_MenuBarBg] = {0.085f, 0.09f, 0.105f, 1};
    colors[ImGuiCol_Border] = {54 / 255.0f, 59 / 255.0f, 68 / 255.0f, 1};
    colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    colors[ImGuiCol_FrameBg] = {42 / 255.0f, 46 / 255.0f, 52 / 255.0f, 1};
    colors[ImGuiCol_FrameBgHovered] = {0.205f, 0.22f, 0.25f, 1};
    colors[ImGuiCol_FrameBgActive] = {0.23f, 0.255f, 0.30f, 1};
    colors[ImGuiCol_Button] = colors[ImGuiCol_FrameBg];
    colors[ImGuiCol_ButtonHovered] = colors[ImGuiCol_FrameBgHovered];
    colors[ImGuiCol_ButtonActive] = colors[ImGuiCol_FrameBgActive];
    colors[ImGuiCol_Header] = {0.19f, 0.25f, 0.32f, 1};
    colors[ImGuiCol_HeaderHovered] = {0.23f, 0.29f, 0.37f, 1};
    colors[ImGuiCol_HeaderActive] = {0.25f, 0.33f, 0.43f, 1};
    colors[ImGuiCol_CheckMark] = {106 / 255.0f, 169 / 255.0f, 255 / 255.0f, 1};
    colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SliderGrabActive] = {0.61f, 0.80f, 1, 1};
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_MenuBarBg];
    colors[ImGuiCol_TitleBgActive] = {0.13f, 0.14f, 0.16f, 1};
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_Tab] = colors[ImGuiCol_MenuBarBg];
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_FrameBgHovered];
    colors[ImGuiCol_TabSelected] = {0.16f, 0.175f, 0.20f, 1};
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TabDimmed] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabDimmedSelected] = colors[ImGuiCol_TabSelected];
    colors[ImGuiCol_TabDimmedSelectedOverline] = {0.35f, 0.42f, 0.52f, 1};
    colors[ImGuiCol_DockingPreview] = {0.44f, 0.69f, 0.96f, 0.35f};
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    colors[ImGuiCol_ScrollbarGrab] = {0.26f, 0.28f, 0.32f, 1};
    colors[ImGuiCol_ScrollbarGrabHovered] = {0.35f, 0.38f, 0.43f, 1};
    colors[ImGuiCol_ScrollbarGrabActive] = {0.44f, 0.47f, 0.53f, 1};
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_Border];
    colors[ImGuiCol_TableBorderLight] = {0.17f, 0.18f, 0.21f, 1};
    colors[ImGuiCol_TableHeaderBg] = {0.14f, 0.15f, 0.175f, 1};
    colors[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.025f};
    colors[ImGuiCol_TextSelectedBg] = {0.34f, 0.55f, 0.80f, 0.35f};
}
ImVec4 EditorPhaseColor(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::Ready         : return {0.43f, 0.78f, 0.61f, 1};
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
    case EditorGlyph::More   : return ICON_FA_ELLIPSIS;
    case EditorGlyph::Clear  : return ICON_FA_TRASH_CAN;
    case EditorGlyph::Step   : return ICON_FA_FORWARD_STEP;
    }
    return "";
}
