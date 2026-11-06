#include "ToolUi/ToolTheme.h"
#include <algorithm>

namespace {
ImGuiContext* FontsContext = nullptr;
ImGuiContext* ColorsContext = nullptr;
const ToolColorPalette* ActiveColors = nullptr;
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

const ToolColorPalette& ToolColors() {
    return ActiveColors && ColorsContext == ImGui::GetCurrentContext() ? *ActiveColors : CabinetToolColors();
}
void RegisterToolColors(const ToolColorPalette* colors) {
    if (!colors && ColorsContext != ImGui::GetCurrentContext()) return;
    ColorsContext = colors ? ImGui::GetCurrentContext() : nullptr;
    ActiveColors = colors;
}

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
    ApplyToolPalette(style.Colors, ToolColors());
}

ImVec4 ToolSurfaceColor(ToolSurface surface) {
    switch (surface) {
    case ToolSurface::Header     : return ToolColors().window;
    case ToolSurface::Toolbar    : return ToolColors().window;
    case ToolSurface::Workspace  : return ToolColors().window;
    case ToolSurface::Content    : return ToolColors().content;
    case ToolSurface::Utility    : return ToolColors().panel;
    case ToolSurface::Status     : return ToolColors().window;
    case ToolSurface::PanelHeader: return ToolColors().elevated;
    }
    return {};
}
ImVec4 ToolToneColor(ToolTone tone) {
    switch (tone) {
    case ToolTone::Accent     : return ToolColors().accent;
    case ToolTone::Success    : return ToolColors().success;
    case ToolTone::Warning    : return ToolColors().warning;
    case ToolTone::Error      : return ToolColors().error;
    case ToolTone::Information: return ToolColors().information;
    }
    return {};
}
