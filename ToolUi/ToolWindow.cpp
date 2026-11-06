#include "ToolUi/ToolWindow.h"

namespace {
constexpr Color4 ToolWindowBackground{15, 14, 13, 255}; // ToolPalette::Window
} // namespace
GameSettings ToolGameSettings(const std::filesystem::path& applicationId, const WindowSettings& window) {
    GameSettings settings;
    auto& project = settings.project;
    const auto id = applicationId.generic_u8string();
    project.applicationId.assign(id.begin(), id.end());
    project.title = window.title ? window.title : "";
    project.windowWidth = window.width;
    project.windowHeight = window.height;
    project.targetFps = window.targetFps;
    project.windowMinWidth = window.minWidth;
    project.windowMinHeight = window.minHeight;
    project.resizable = window.resizable;
    project.maximized = window.maximized;
    project.background = ToolWindowBackground;
    settings.windowVisible = window.visible;
    settings.alwaysRun = window.alwaysRun;
    settings.persistentOptions = false;
    return settings;
}
