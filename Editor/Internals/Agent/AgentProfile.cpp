#include "Editor/Internals/Agent/AgentProfile.h"
#include <algorithm>
#include <cctype>

namespace {
std::string_view Trimmed(std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}
} // namespace

std::string AgentCommand(const AgentPreferences& preferences) {
    if (preferences.kind == AgentKind::Custom) return std::string(Trimmed(preferences.customCommand));
    return AgentKindInfo(preferences.kind).command;
}
std::string AgentProgram(std::string_view command) {
    command = Trimmed(command);
    if (command.starts_with('"')) {
        const auto end = command.find('"', 1);
        return std::string(command.substr(1, end == std::string_view::npos ? std::string_view::npos : end - 1));
    }
    return std::string(command.substr(0, command.find_first_of(" \t")));
}
std::string AgentCommandLine(std::string_view shell, std::string_view command) {
    // PowerShell runs the rest of the line as one command (-Command must be last); -NoExit keeps the session open.
    return std::string(shell) + " -NoExit -Command " + std::string(Trimmed(command));
}

std::optional<AgentKind> DetectRunningAgent(const std::vector<std::string>& processNames,
                                          std::optional<AgentKind> current) {
    bool claude = false;
    bool codex = false;
    for (auto name : processNames) {
        std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        claude |= name == "claude.exe" || name == "claude";
        codex |= name == "codex.exe" || name == "codex";
    }
    if (claude && current == AgentKind::ClaudeCode) return current;
    if (codex && current == AgentKind::Codex) return current;
    if (claude) return AgentKind::ClaudeCode;
    if (codex) return AgentKind::Codex;
    return std::nullopt;
}
