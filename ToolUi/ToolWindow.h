#pragma once
#include <filesystem>
#include "Runtime/GameSettings.h"

// GameSettings for a tool window (editor, launcher): no options file, canvas content, audio or game font of its own;
// the whole window is drawn by ImGui in Game::DrawToolOverlay through a ToolUiContext.
[[nodiscard]] GameSettings ToolGameSettings(const std::filesystem::path& applicationId, const WindowSettings& window);
