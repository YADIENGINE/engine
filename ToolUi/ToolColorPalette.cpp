#include "ToolUi/ToolColorPalette.h"
#include <algorithm>
#include "ToolUi/ToolTheme.h"

namespace {
ImVec4 Rgb(int red, int green, int blue) {
    return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f,
            1.0f};
}
} // namespace

ToolColorPalette::ToolColorPalette()
    : window(ToolPalette::Window)
    , panel(ToolPalette::Panel)
    , elevated(ToolPalette::Elevated)
    , content(ToolPalette::Content)
    , control(ToolPalette::Control)
    , hover(ToolPalette::Hover)
    , pressed(ToolPalette::Pressed)
    , selected(ToolPalette::Selected)
    , accent(ToolPalette::Accent)
    , accentHover(ToolPalette::AccentHover)
    , accentPressed(ToolPalette::AccentPressed)
    , accentText(ToolPalette::AccentText)
    , text(ToolPalette::Text)
    , secondary(ToolPalette::Secondary)
    , muted(ToolPalette::Muted)
    , disabled(ToolPalette::Disabled)
    , border(ToolPalette::Border)
    , separator(ToolPalette::Separator)
    , sliderGrab(Rgb(0x5a, 0x55, 0x4d))
    , success(Rgb(0x8f, 0xc7, 0x9a))
    , warning(ToolPalette::Accent)
    , error(Rgb(0xe8, 0x85, 0x8f))
    , information(ToolPalette::Secondary)
    , agent(Rgb(0xa3, 0xae, 0xf5))
    , activity(Rgb(0x83, 0xc4, 0xd6))
    , stopText(Rgb(0xf4, 0xc5, 0xca))
    , successHover(Rgb(0xa5, 0xd6, 0xaf))
    , successPressed(Rgb(0x7a, 0xb3, 0x86))
    , successText(Rgb(0x14, 0x25, 0x1a))
    , assetFont(Rgb(0xd0, 0xcb, 0x7c))
    , tableRowAlt(1, 1, 1, 0.02f)
    , modalDim(0, 0, 0, 0.62f)
    , navWindowingHighlight(1, 1, 1, 0.70f)
    , navWindowingDim(0.80f, 0.80f, 0.80f, 0.20f) {}

const ToolColorPalette& CabinetToolColors() {
    static const ToolColorPalette Colors;
    return Colors;
}
void ApplyToolPalette(std::span<ImVec4, ImGuiCol_COUNT> colors, const ToolColorPalette& palette) {
    const auto accent = palette.accent;
    constexpr ImVec4 Transparent{0, 0, 0, 0};
    std::ranges::fill(colors, Transparent); // Includes transparent table rows and border shadows.
    colors[ImGuiCol_Text] = palette.text;
    colors[ImGuiCol_TextDisabled] = palette.secondary; // Metadata role; disabled controls use Disabled.
    colors[ImGuiCol_WindowBg] = palette.panel;
    colors[ImGuiCol_ChildBg] = Transparent;
    colors[ImGuiCol_PopupBg] = palette.elevated;
    colors[ImGuiCol_MenuBarBg] = palette.window;
    colors[ImGuiCol_Border] = palette.border;
    colors[ImGuiCol_BorderShadow] = Transparent;
    colors[ImGuiCol_FrameBg] = palette.content;
    colors[ImGuiCol_FrameBgHovered] = palette.elevated;
    colors[ImGuiCol_FrameBgActive] = palette.content;
    colors[ImGuiCol_Button] = palette.control;
    colors[ImGuiCol_ButtonHovered] = palette.hover;
    colors[ImGuiCol_ButtonActive] = palette.pressed;
    colors[ImGuiCol_Header] = palette.selected;
    colors[ImGuiCol_HeaderHovered] = palette.control;
    colors[ImGuiCol_HeaderActive] = palette.selected;
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = palette.sliderGrab; // Neutral at rest: the value is written over the grab
    colors[ImGuiCol_SliderGrabActive] = accent;
    colors[ImGuiCol_InputTextCursor] = accent;
    colors[ImGuiCol_TitleBg] = palette.window;
    colors[ImGuiCol_TitleBgActive] = palette.window;
    colors[ImGuiCol_TitleBgCollapsed] = palette.window;
    colors[ImGuiCol_Tab] = palette.window;
    colors[ImGuiCol_TabHovered] = palette.elevated;
    colors[ImGuiCol_TabSelected] = palette.panel;
    colors[ImGuiCol_TabSelectedOverline] = accent; // The focused pane; other panes stay quiet
    colors[ImGuiCol_TabDimmed] = palette.window;
    colors[ImGuiCol_TabDimmedSelected] = palette.panel;
    colors[ImGuiCol_TabDimmedSelectedOverline] = Transparent;
    colors[ImGuiCol_DockingPreview] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_DockingEmptyBg] = palette.window;
    colors[ImGuiCol_ScrollbarBg] = Transparent;
    colors[ImGuiCol_ScrollbarGrab] = palette.control;
    colors[ImGuiCol_ScrollbarGrabHovered] = palette.hover;
    colors[ImGuiCol_ScrollbarGrabActive] = palette.muted;
    colors[ImGuiCol_Separator] = palette.separator;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accent;
    colors[ImGuiCol_ResizeGrip] = Transparent;
    colors[ImGuiCol_ResizeGripHovered] = accent;
    colors[ImGuiCol_ResizeGripActive] = accent;
    colors[ImGuiCol_TableBorderStrong] = palette.separator;
    colors[ImGuiCol_TableBorderLight] = palette.separator;
    colors[ImGuiCol_TableHeaderBg] = palette.elevated;
    colors[ImGuiCol_TableRowBgAlt] = palette.tableRowAlt;
    colors[ImGuiCol_TextSelectedBg] = {accent.x, accent.y, accent.z, 0.25f};
    colors[ImGuiCol_TextLink] = accent;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_DragDropTargetBg] = {accent.x, accent.y, accent.z, 0.12f};
    colors[ImGuiCol_TreeLines] = palette.separator;
    colors[ImGuiCol_UnsavedMarker] = accent;
    colors[ImGuiCol_ModalWindowDimBg] = palette.modalDim;
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = palette.accentHover;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = palette.accentHover;
    colors[ImGuiCol_NavWindowingHighlight] = palette.navWindowingHighlight;
    colors[ImGuiCol_NavWindowingDimBg] = palette.navWindowingDim;
}
