#include "ToolUi/ToolTheme.h"
#include <algorithm>

namespace {
ImGuiContext* FontsContext = nullptr;
ImFont* UiFont = nullptr;
ImFont* MediumFont = nullptr;
ImFont* HeadingFont = nullptr;
ImFont* TechnicalFont = nullptr;
ImVec4 Rgb(int red, int green, int blue) {
    return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f,
            1.0f};
}
} // namespace

const ImVec4 ToolPalette::Window = Rgb(0x1e, 0x1a, 0x22);
const ImVec4 ToolPalette::Panel = Rgb(0x2c, 0x26, 0x33);
const ImVec4 ToolPalette::Content = Rgb(0x16, 0x13, 0x1b);
const ImVec4 ToolPalette::Control = Rgb(0x3d, 0x35, 0x47);
const ImVec4 ToolPalette::Hover = Rgb(0x4b, 0x41, 0x56);
const ImVec4 ToolPalette::Pressed = Rgb(0x34, 0x2c, 0x3d);
const ImVec4 ToolPalette::Selected = Rgb(0x49, 0x3b, 0x50);
const ImVec4 ToolPalette::Accent = Rgb(0xf4, 0xaf, 0x65);
const ImVec4 ToolPalette::AccentHover = Rgb(0xff, 0xc1, 0x83);
const ImVec4 ToolPalette::AccentPressed = Rgb(0xde, 0x98, 0x50);
const ImVec4 ToolPalette::AccentText = Rgb(0x30, 0x20, 0x13);
const ImVec4 ToolPalette::Text = Rgb(0xf0, 0xea, 0xf1);
const ImVec4 ToolPalette::Secondary = Rgb(0xb7, 0xab, 0xbf);
const ImVec4 ToolPalette::Muted = Rgb(0x92, 0x87, 0x9b);
const ImVec4 ToolPalette::Disabled = Rgb(0x74, 0x6c, 0x7e);
const ImVec4 ToolPalette::Border = Rgb(0x5a, 0x4c, 0x65);
const ImVec4 ToolPalette::Separator = Rgb(0x40, 0x37, 0x47);

void RegisterToolFonts(ImFont* ui, ImFont* technical, ImFont* medium, ImFont* heading) {
    FontsContext = ImGui::GetCurrentContext();
    UiFont = ui;
    MediumFont = medium ? medium : ui;
    HeadingFont = heading ? heading : ui;
    TechnicalFont = technical;
    ImGui::GetIO().FontDefault = ui;
}
void ClearToolFonts() {
    if (FontsContext != ImGui::GetCurrentContext()) return;
    FontsContext = nullptr;
    UiFont = nullptr;
    MediumFont = nullptr;
    HeadingFont = nullptr;
    TechnicalFont = nullptr;
}
ImFont* ToolFont(ToolFontRole role) {
    if (FontsContext == ImGui::GetCurrentContext()) {
        ImFont* font = nullptr;
        switch (role) {
        case ToolFontRole::Ui       : font = UiFont; break;
        case ToolFontRole::Medium   : font = MediumFont; break;
        case ToolFontRole::Heading  : font = HeadingFont; break;
        case ToolFontRole::Technical: font = TechnicalFont; break;
        }
        if (font) return font;
    }
    // Small UI fixtures and other ImGui contexts do not register tool fonts.
    return ImGui::GetIO().FontDefault ? ImGui::GetIO().FontDefault : ImGui::GetFont();
}
float ToolUiScale() {
    const auto& style = ImGui::GetStyle();
    return std::max(0.5f, style.FontScaleMain * style.FontScaleDpi);
}

void ApplyToolTheme() {
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowPadding = {ToolMetrics::Padding, ToolMetrics::Padding};
    style.FramePadding = {10, (ToolMetrics::ControlHeight - ToolMetrics::BodySize) * 0.5f};
    style.ItemSpacing = {ToolMetrics::Gap, ToolMetrics::Gap};
    style.ItemInnerSpacing = {ToolMetrics::Gap, ToolMetrics::Space};
    style.CellPadding = {ToolMetrics::Gap, ToolMetrics::Space};
    style.WindowRounding = ToolMetrics::Radius;
    style.ChildRounding = ToolMetrics::Radius;
    style.FrameRounding = ToolMetrics::Radius;
    style.PopupRounding = ToolMetrics::Radius;
    style.GrabRounding = ToolMetrics::Radius;
    style.TabRounding = ToolMetrics::Radius;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.ScrollbarSize = 12;
    style.ScrollbarRounding = ToolMetrics::Radius;
    style.GrabMinSize = 8;
    style.DisabledAlpha = 0.45f;
    auto& colors = style.Colors;
    // Arcade Cabinet: dark plum surfaces, warm amber focus and neutral selections.
    const auto accent = ToolPalette::Accent;
    colors[ImGuiCol_Text] = ToolPalette::Text;
    colors[ImGuiCol_TextDisabled] = ToolPalette::Secondary; // Metadata role; disabled controls use Disabled.
    colors[ImGuiCol_WindowBg] = ToolPalette::Window;
    colors[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    colors[ImGuiCol_PopupBg] = ToolPalette::Panel;
    colors[ImGuiCol_MenuBarBg] = ToolSurfaceColor(ToolSurface::Header);
    colors[ImGuiCol_Border] = ToolPalette::Border;
    colors[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    colors[ImGuiCol_FrameBg] = ToolPalette::Content;
    colors[ImGuiCol_FrameBgHovered] = ToolPalette::Hover;
    colors[ImGuiCol_FrameBgActive] = ToolPalette::Selected;
    colors[ImGuiCol_Button] = ToolPalette::Control;
    colors[ImGuiCol_ButtonHovered] = ToolPalette::Hover;
    colors[ImGuiCol_ButtonActive] = ToolPalette::Pressed;
    colors[ImGuiCol_Header] = ToolPalette::Selected;
    colors[ImGuiCol_HeaderHovered] = ToolPalette::Hover;
    colors[ImGuiCol_HeaderActive] = ToolPalette::Selected;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SliderGrabActive] = ToolPalette::AccentHover;
    colors[ImGuiCol_InputTextCursor] = accent;
    colors[ImGuiCol_TitleBg] = colors[ImGuiCol_MenuBarBg];
    colors[ImGuiCol_TitleBgActive] = ToolPalette::Selected;
    colors[ImGuiCol_TitleBgCollapsed] = colors[ImGuiCol_TitleBg];
    colors[ImGuiCol_Tab] = ToolSurfaceColor(ToolSurface::Header);
    colors[ImGuiCol_TabHovered] = ToolPalette::Hover;
    colors[ImGuiCol_TabSelected] = ToolPalette::Selected;
    colors[ImGuiCol_TabSelectedOverline] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TabDimmed] = colors[ImGuiCol_Tab];
    colors[ImGuiCol_TabDimmedSelected] = colors[ImGuiCol_TabSelected];
    colors[ImGuiCol_TabDimmedSelectedOverline] = ToolPalette::Muted;
    colors[ImGuiCol_DockingPreview] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_DockingEmptyBg] = colors[ImGuiCol_ChildBg];
    colors[ImGuiCol_ScrollbarBg] = ToolPalette::Window;
    colors[ImGuiCol_ScrollbarGrab] = ToolPalette::Control;
    colors[ImGuiCol_ScrollbarGrabHovered] = ToolPalette::Hover;
    colors[ImGuiCol_ScrollbarGrabActive] = ToolPalette::Muted;
    colors[ImGuiCol_Separator] = ToolPalette::Separator;
    colors[ImGuiCol_SeparatorHovered] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_SeparatorActive] = colors[ImGuiCol_CheckMark];
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_Border];
    colors[ImGuiCol_TableBorderLight] = ToolPalette::Separator;
    colors[ImGuiCol_TableHeaderBg] = ToolSurfaceColor(ToolSurface::PanelHeader);
    colors[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.02f};
    colors[ImGuiCol_TextSelectedBg] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_TextLink] = ToolToneColor(ToolTone::Information);
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_DragDropTargetBg] = {accent.x, accent.y, accent.z, 0.12f};
    colors[ImGuiCol_TreeLines] = colors[ImGuiCol_Border];
    colors[ImGuiCol_UnsavedMarker] = ToolToneColor(ToolTone::Warning);
    colors[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.55f};
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = colors[ImGuiCol_SliderGrabActive];
    colors[ImGuiCol_PlotHistogram] = ToolToneColor(ToolTone::Information);
    colors[ImGuiCol_PlotHistogramHovered] = ToolToneColor(ToolTone::Warning);
}
ImVec4 ToolSurfaceColor(ToolSurface surface) {
    switch (surface) {
    case ToolSurface::Header     : return ToolPalette::Panel;
    case ToolSurface::Toolbar    : return ToolPalette::Panel;
    case ToolSurface::Workspace  : return ToolPalette::Panel;
    case ToolSurface::Content    : return ToolPalette::Content;
    case ToolSurface::Utility    : return ToolPalette::Panel;
    case ToolSurface::Status     : return ToolPalette::Panel;
    case ToolSurface::PanelHeader: return ToolPalette::Control;
    }
    return {};
}
ImVec4 ToolToneColor(ToolTone tone) {
    switch (tone) {
    case ToolTone::Accent     : return ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    case ToolTone::Success    : return Rgb(0x89, 0xc9, 0xa4);
    case ToolTone::Warning    : return ToolPalette::Accent;
    case ToolTone::Error      : return Rgb(0xdb, 0x81, 0x8d);
    case ToolTone::Information: return ToolPalette::Secondary;
    }
    return {};
}
