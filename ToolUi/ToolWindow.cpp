#include "ToolUi/ToolWindow.h"

namespace {
constexpr Color4 ToolWindowBackground{20, 22, 28, 255};
} // namespace
GameSettings ToolGameSettings(const std::filesystem::path& applicationId, const WindowSettings& window) {
    GameSettings settings;
    settings.applicationId = applicationId;
    settings.persistentOptions = false;
    settings.window = window;
    settings.background = ToolWindowBackground;
    return settings;
}
