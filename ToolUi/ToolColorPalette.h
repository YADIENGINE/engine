#pragma once
#include <imgui.h>
#include <span>

// Color data only: palettes cannot change typography, dimensions, input or animation.
// Default construction reproduces Cabinet. Copy it to author another complete palette.
struct ToolColorPalette {
    ToolColorPalette();
    ImVec4 window, panel, elevated, content;
    ImVec4 control, hover, pressed, selected;
    ImVec4 accent, accentHover, accentPressed, accentText;
    ImVec4 text, secondary, muted, disabled, border, separator, sliderGrab;
    ImVec4 success, warning, error, information;
    ImVec4 agent, activity, stopText, successHover, successPressed, successText, assetFont;
    ImVec4 tableRowAlt, modalDim, navWindowingHighlight, navWindowingDim;
};

[[nodiscard]] const ToolColorPalette& CabinetToolColors();
// The destination deliberately exposes only color slots, never the rest of ImGuiStyle.
void ApplyToolPalette(std::span<ImVec4, ImGuiCol_COUNT> colors, const ToolColorPalette& palette);
