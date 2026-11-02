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

const ImVec4 ToolPalette::Window = Rgb(0x0f, 0x0e, 0x0d);
const ImVec4 ToolPalette::Panel = Rgb(0x1c, 0x1a, 0x18);
const ImVec4 ToolPalette::Elevated = Rgb(0x24, 0x22, 0x1f);
const ImVec4 ToolPalette::Content = Rgb(0x0a, 0x09, 0x08);
const ImVec4 ToolPalette::Control = Rgb(0x2b, 0x28, 0x25);
const ImVec4 ToolPalette::Hover = Rgb(0x36, 0x32, 0x2e);
const ImVec4 ToolPalette::Pressed = Rgb(0x24, 0x22, 0x1f);
const ImVec4 ToolPalette::Selected = Rgb(0x33, 0x2c, 0x21);
const ImVec4 ToolPalette::Accent = Rgb(0xf4, 0xaf, 0x65);
const ImVec4 ToolPalette::AccentHover = Rgb(0xff, 0xc1, 0x83);
const ImVec4 ToolPalette::AccentPressed = Rgb(0xde, 0x98, 0x50);
const ImVec4 ToolPalette::AccentText = Rgb(0x2a, 0x1a, 0x0a);
const ImVec4 ToolPalette::Text = Rgb(0xed, 0xe8, 0xe0);
const ImVec4 ToolPalette::Secondary = Rgb(0xac, 0xa6, 0x9d);
const ImVec4 ToolPalette::Muted = Rgb(0x8c, 0x86, 0x7d);
const ImVec4 ToolPalette::Disabled = Rgb(0x7a, 0x74, 0x6c);
const ImVec4 ToolPalette::Border = Rgb(0x3a, 0x36, 0x32);
const ImVec4 ToolPalette::Separator = Rgb(0x2f, 0x2c, 0x28);

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
    style.TabRounding = 0; // A selected tab flows into its pane
    // Tone separates regions; only what floats above them (popups, dialogs) draws a line.
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 0;
    style.TabBorderSize = 0;
    style.TabBarBorderSize = 0;
    style.TabBarOverlineSize = 2;
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.TabCloseButtonMinWidthSelected = 0; // Close buttons appear on hover
    style.TabCloseButtonMinWidthUnselected = 0;
    style.DockingSeparatorSize = 2;
    style.ScrollbarSize = 10;
    style.ScrollbarRounding = ToolMetrics::Radius;
    style.GrabMinSize = 8;
    style.DisabledAlpha = 0.45f;
    auto& colors = style.Colors;
    const auto accent = ToolPalette::Accent;
    constexpr ImVec4 Transparent{0, 0, 0, 0};
    colors[ImGuiCol_Text] = ToolPalette::Text;
    colors[ImGuiCol_TextDisabled] = ToolPalette::Secondary; // Metadata role; disabled controls use Disabled.
    colors[ImGuiCol_WindowBg] = ToolPalette::Panel;
    colors[ImGuiCol_ChildBg] = Transparent;
    colors[ImGuiCol_PopupBg] = ToolPalette::Elevated;
    colors[ImGuiCol_MenuBarBg] = ToolPalette::Window;
    colors[ImGuiCol_Border] = ToolPalette::Border;
    colors[ImGuiCol_BorderShadow] = Transparent;
    colors[ImGuiCol_FrameBg] = ToolPalette::Content;
    colors[ImGuiCol_FrameBgHovered] = ToolPalette::Elevated;
    colors[ImGuiCol_FrameBgActive] = ToolPalette::Content;
    colors[ImGuiCol_Button] = ToolPalette::Control;
    colors[ImGuiCol_ButtonHovered] = ToolPalette::Hover;
    colors[ImGuiCol_ButtonActive] = ToolPalette::Pressed;
    colors[ImGuiCol_Header] = ToolPalette::Selected;
    colors[ImGuiCol_HeaderHovered] = ToolPalette::Control;
    colors[ImGuiCol_HeaderActive] = ToolPalette::Selected;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = Rgb(0x5a, 0x55, 0x4d); // Neutral at rest: the value is written over the grab
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_InputTextCursor] = accent;
    colors[ImGuiCol_TitleBg] = ToolPalette::Window;
    colors[ImGuiCol_TitleBgActive] = ToolPalette::Window;
    colors[ImGuiCol_TitleBgCollapsed] = ToolPalette::Window;
    colors[ImGuiCol_Tab] = ToolPalette::Window;
    colors[ImGuiCol_TabHovered] = ToolPalette::Elevated;
    colors[ImGuiCol_TabSelected] = ToolPalette::Panel;
    colors[ImGuiCol_TabSelectedOverline] = accent; // The focused pane; other panes stay quiet
    colors[ImGuiCol_TabDimmed] = ToolPalette::Window;
    colors[ImGuiCol_TabDimmedSelected] = ToolPalette::Panel;
    colors[ImGuiCol_TabDimmedSelectedOverline] = Transparent;
    colors[ImGuiCol_DockingPreview] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_DockingEmptyBg] = ToolPalette::Window;
    colors[ImGuiCol_ScrollbarBg] = Transparent;
    colors[ImGuiCol_ScrollbarGrab] = ToolPalette::Control;
    colors[ImGuiCol_ScrollbarGrabHovered] = ToolPalette::Hover;
    colors[ImGuiCol_ScrollbarGrabActive] = ToolPalette::Muted;
    colors[ImGuiCol_Separator] = ToolPalette::Separator;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = Transparent;
    colors[ImGuiCol_ResizeGripHovered] = accent;
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_TableBorderStrong] = ToolPalette::Separator;
    colors[ImGuiCol_TableBorderLight] = ToolPalette::Separator;
    colors[ImGuiCol_TableHeaderBg] = ToolPalette::Elevated;
    colors[ImGuiCol_TableRowBgAlt] = {1, 1, 1, 0.02f};
    colors[ImGuiCol_TextSelectedBg] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_TextLink] = accent;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_DragDropTargetBg] = {accent.x, accent.y, accent.z, 0.12f};
    colors[ImGuiCol_TreeLines] = ToolPalette::Separator;
    colors[ImGuiCol_UnsavedMarker] = accent;
    colors[ImGuiCol_ModalWindowDimBg] = {0, 0, 0, 0.62f};
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = ToolPalette::AccentHover;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = ToolPalette::AccentHover;
}
ImVec4 ToolSurfaceColor(ToolSurface surface) {
    switch (surface) {
    case ToolSurface::Header     : return ToolPalette::Window;
    case ToolSurface::Toolbar    : return ToolPalette::Window;
    case ToolSurface::Workspace  : return ToolPalette::Window;
    case ToolSurface::Content    : return ToolPalette::Content;
    case ToolSurface::Utility    : return ToolPalette::Panel;
    case ToolSurface::Status     : return ToolPalette::Window;
    case ToolSurface::PanelHeader: return ToolPalette::Elevated;
    }
    return {};
}
ImVec4 ToolToneColor(ToolTone tone) {
    switch (tone) {
    case ToolTone::Accent     : return ToolPalette::Accent;
    case ToolTone::Success    : return Rgb(0x8f, 0xc7, 0x9a);
    case ToolTone::Warning    : return ToolPalette::Accent;
    case ToolTone::Error      : return Rgb(0xe8, 0x85, 0x8f);
    case ToolTone::Information: return ToolPalette::Secondary;
    }
    return {};
}
