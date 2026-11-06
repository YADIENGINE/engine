#pragma once
#include <imgui.h>
#include "ToolUi/ToolColorPalette.h"

// Shared design tokens are logical pixels at 100% interface size and 96 DPI.
struct ToolMetrics {
    static constexpr float Space = 4;
    static constexpr float Gap = 8;
    static constexpr float Padding = 12;
    static constexpr float SectionGap = 16;
    static constexpr float Radius = 3;
    static constexpr float ControlHeight = 30;
    static constexpr float PanelHeaderPadding = 5;
    static constexpr float BodySize = 14;
    static constexpr float CaptionSize = 12;
    static constexpr float SectionSize = 13;
    static constexpr float HeadingSize = 18;
    static constexpr float TitleSize = 28;
    static constexpr float IconSize = 14;
};

enum class ToolFontRole { Ui, Medium, Heading, Technical };
// Accent: selection / focus; Success, Warning, Error, Information: state and message colors.
enum class ToolTone { Accent, Success, Warning, Error, Information };
enum class ToolSurface { Header, Toolbar, Workspace, Content, Utility, Status, PanelHeader };

// Fixed Cabinet colors retained for existing tools and content backdrops. New themed UI uses ToolColors().
struct ToolPalette {
    static const ImVec4 Window;   // The darkest frame: bars, and the gaps between panes
    static const ImVec4 Panel;    // Panes
    static const ImVec4 Elevated; // Popups and hover over a pane
    static const ImVec4 Content;  // Recessed wells: inputs, the viewport, lists of content
    static const ImVec4 Control;
    static const ImVec4 Hover;
    static const ImVec4 Pressed;
    static const ImVec4 Selected;
    static const ImVec4 Accent;
    static const ImVec4 AccentHover;
    static const ImVec4 AccentPressed;
    static const ImVec4 AccentText;
    static const ImVec4 Text;
    static const ImVec4 Secondary;
    static const ImVec4 Muted;
    static const ImVec4 Disabled;
    static const ImVec4 Border; // Only what floats: popups and dialogs
    static const ImVec4 Separator;
};
void ApplyToolTheme();
// The active context's palette, or Cabinet for contexts without a ToolUiContext.
[[nodiscard]] const ToolColorPalette& ToolColors();
// Context lifetime binding, owned by ToolUiContext just like the font roles below.
void RegisterToolColors(const ToolColorPalette* colors);
// The single ToolUiContext owns these non-owning roles and clears them before its ImGui context is destroyed.
void RegisterToolFonts(ImFont* ui, ImFont* technical, ImFont* medium = nullptr, ImFont* heading = nullptr);
void ClearToolFonts();
[[nodiscard]] ImFont* ToolFont(ToolFontRole role);
[[nodiscard]] const char* ToolFontLicense();
[[nodiscard]] float ToolUiScale();
[[nodiscard]] ImVec4 ToolToneColor(ToolTone tone);
[[nodiscard]] ImVec4 ToolSurfaceColor(ToolSurface surface);
