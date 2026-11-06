#pragma once
#include <imgui.h>
#include "ToolUi/ToolColorPalette.h"

struct ToolUiOptions {
    bool docking = false;            // ImGui docking (the editor's workspace)
    bool keyboardNavigation = false; // Off when the keyboard also drives something else (an editor scene)
};

// The ImGui context of a tool window: rlImGui backend, embedded IBM Plex Sans + Font Awesome, IBM Plex Mono for
// technical values, theme, native title bar colors and scaling. Create after the window (Game).
class ToolUiContext {
public:
    explicit ToolUiContext(const ToolUiOptions& options = {});
    ~ToolUiContext();
    // Call between frames. Updates only colors (including the unscaled baseline and native caption).
    void SetPalette(const ToolColorPalette& palette);
    // scalePercent: the user's interface size; 100 is the default scale, multiplied by the monitor's DPI.
    void BeginFrame(int scalePercent);
    void EndFrame() const;

private:
    ToolColorPalette _palette;
    ImGuiStyle _baseStyle;
    float _scale = 0;

public:
    ToolUiContext(const ToolUiContext&) = delete;
    ToolUiContext& operator=(const ToolUiContext&) = delete;
};
