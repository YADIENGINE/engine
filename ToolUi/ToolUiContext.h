#pragma once
#include <imgui.h>

struct ToolUiOptions {
    bool docking = false;            // ImGui docking (the editor's workspace)
    bool keyboardNavigation = false; // Off when the keyboard also drives something else (an editor scene)
};

// The ImGui context of a tool window: rlImGui backend, Segoe UI with embedded Noto Sans fallback + Font Awesome,
// Consolas for technical values (embedded fallback), theme and scaling. Create after the window (Game).
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
