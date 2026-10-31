#pragma once
#include <filesystem>
#include <string>

// "Back to project list": a launcher that starts the editor puts its windowed executable in ENGINE_LAUNCHER
// (Docs/ToolsContract.md). The editor knows nothing else about it.

// The launcher executable from ENGINE_LAUNCHER; empty when the editor was started without one or it does not exist.
[[nodiscard]] std::filesystem::path LauncherFromEnvironment();
// Starts the launcher independent of the editor; an error message, empty on success.
[[nodiscard]] std::string StartLauncher(const std::filesystem::path& launcher);
