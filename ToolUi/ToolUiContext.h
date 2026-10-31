#pragma once
#include <imgui.h>

struct ToolUiOptions {
    bool docking = false;            // ImGui docking (the editor's workspace)
    bool keyboardNavigation = false; // Off when the keyboard also drives something else (the editor's preview)
};

// The ImGui context of a tool window: rlImGui backend, tool fonts (Segoe UI + Font Awesome, Consolas; embedded font
// when a system font is missing), theme and scaling. Create after the window (Game) and destroy before it.
class ToolUiContext {
public:
    explicit ToolUiContext(const ToolUiOptions& options = {});
    ~ToolUiContext();
    // scalePercent: the user's interface size; 100 is the default scale, multiplied by the monitor's DPI.
    void BeginFrame(int scalePercent);
    void EndFrame() const;

private:
    ImGuiStyle _baseStyle;
    float _scale = 0;

public:
    ToolUiContext(const ToolUiContext&) = delete;
    ToolUiContext& operator=(const ToolUiContext&) = delete;
};
