#include "Editor/Ui/Style/EditorTheme.h"
#include <extras/IconsFontAwesome6.h>
#include <algorithm>

namespace {
constexpr float BaseFontSize = 14.0f;
ImGuiContext* FontsContext = nullptr;
ImFont* UiFont = nullptr;
ImFont* TechnicalFont = nullptr;
ImVec4 Rgb(int red, int green, int blue) {
    return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f,
            1.0f};
}
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
    // Midnight Teal: layered blue-black surfaces and restrained semantic accents.
    const auto base = Rgb(0x10, 0x15, 0x1d);
    const auto panel = Rgb(0x19, 0x21, 0x2c);
    const auto raised = Rgb(0x22, 0x2e, 0x3d);
    const auto accent = Rgb(0x59, 0xd8, 0xc5);
    colors[ImGuiCol_Text] = Rgb(0xe6, 0xed, 0xf5);
    colors[ImGuiCol_TextDisabled] = Rgb(0x9a, 0xaa, 0xbd);
    colors[ImGuiCol_WindowBg] = panel;
    colors[ImGuiCol_ChildBg] = base;
    colors[ImGuiCol_PopupBg] = raised;
    colors[ImGuiCol_MenuBarBg] = base;
    colors[ImGuiCol_Border] = Rgb(0x30, 0x40, 0x51);
    colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    colors[ImGuiCol_FrameBg] = Rgb(0x12, 0x1b, 0x25);
    colors[ImGuiCol_FrameBgHovered] = Rgb(0x23, 0x33, 0x43);
    colors[ImGuiCol_FrameBgActive] = Rgb(0x29, 0x40, 0x4d);
    colors[ImGuiCol_Button] = raised;
    colors[ImGuiCol_ButtonHovered] = Rgb(0x2b, 0x3c, 0x4c);
    colors[ImGuiCol_ButtonActive] = Rgb(0x30, 0x4b, 0x55);
    colors[ImGuiCol_Header] = Rgb(0x20, 0x3d, 0x42);
    colors[ImGuiCol_HeaderHovered] = Rgb(0x28, 0x4a, 0x50);
    colors[ImGuiCol_HeaderActive] = Rgb(0x30, 0x59, 0x5d);
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SliderGrabActive] = Rgb(0x8a, 0xec, 0xdc);
    colors[ImGuiCol_InputTextCursor] = accent;
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_MenuBarBg];
    colors[ImGuiCol_TitleBgActive] = raised;
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_Tab] = colors[ImGuiCol_MenuBarBg];
    colors[ImGuiCol_TabHovered] = colors[ImGuiCol_Header];
    colors[ImGuiCol_TabSelected] = panel;
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TabDimmed] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabDimmedSelected] = colors[ImGuiCol_TabSelected];
    colors[ImGuiCol_TabDimmedSelectedOverline] = Rgb(0x48, 0x5a, 0x6d);
    colors[ImGuiCol_DockingPreview] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    colors[ImGuiCol_ScrollbarGrab] = Rgb(0x35, 0x46, 0x58);
    colors[ImGuiCol_ScrollbarGrabHovered] = Rgb(0x4b, 0x61, 0x76);
    colors[ImGuiCol_ScrollbarGrabActive] = Rgb(0x60, 0x7a, 0x90);
    colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_Border];
    colors[ImGuiCol_TableBorderLight] = Rgb(0x25, 0x33, 0x42);
    colors[ImGuiCol_TableHeaderBg] = raised;
    colors[ImGuiCol_TableRowBgAlt] = {accent.x, accent.y, accent.z, 0.025f};
    colors[ImGuiCol_TextSelectedBg] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_TextLink] = EditorToneColor(EditorTone::Information);
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_DragDropTargetBg] = {accent.x, accent.y, accent.z, 0.12f};
    colors[ImGuiCol_TreeLines] = colors[ImGuiCol_Border];
    colors[ImGuiCol_UnsavedMarker] = EditorToneColor(EditorTone::Warning);
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = colors[ImGuiCol_SliderGrabActive];
    colors[ImGuiCol_PlotHistogram] = EditorToneColor(EditorTone::Information);
    colors[ImGuiCol_PlotHistogramHovered] = EditorToneColor(EditorTone::Warning);
}
ImVec4 EditorToneColor(EditorTone tone) {
    switch (tone) {
    case EditorTone::Accent     : return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    case EditorTone::Success    : return Rgb(0x80, 0xc9, 0x9b);
    case EditorTone::Warning    : return Rgb(0xe8, 0xbc, 0x76);
    case EditorTone::Error      : return Rgb(0xee, 0x89, 0x92);
    case EditorTone::Information: return Rgb(0x83, 0xb8, 0xf2);
    }
    return {};
}
ImVec4 EditorPhaseColor(EditorPhase phase) {
    switch (phase) {
    case EditorPhase::Ready         : return EditorToneColor(EditorTone::Success);
    case EditorPhase::Error         : return EditorToneColor(EditorTone::Error);
    case EditorPhase::Paused        : return EditorToneColor(EditorTone::Warning);
    case EditorPhase::ReadingCatalog:
    case EditorPhase::Starting      :
    case EditorPhase::Building      : return EditorToneColor(EditorTone::Information);
    case EditorPhase::Stopped       : return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    }
    return {};
}
ImVec4 EditorLevelColor(EditorLogLevel level) {
    if (level == EditorLogLevel::Error) return EditorPhaseColor(EditorPhase::Error);
    if (level == EditorLogLevel::Warning) return EditorPhaseColor(EditorPhase::Paused);
    return EditorToneColor(EditorTone::Information);
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
    }
    return "";
}
