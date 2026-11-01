#pragma once
#include <array>
#include <string>
#include <string_view>

// The agent CLI the Console's agent session runs (Docs/Editor/Usage.md → Agent workspace). The editor only starts the
// developer's own CLI in a terminal; it never calls a model itself. Custom runs any command line.
enum class AgentKind { ClaudeCode, Codex, Custom };

struct AgentKindDefinition {
    AgentKind id;
    const char* key; // Saved in preferences
    const char* name;
    const char* sessionName; // Console session list label
    const char* command;     // Started in the project folder; empty for Custom
    const char* install;     // Where to get it, shown when the command is missing
};

inline constexpr std::array AgentKinds{
    AgentKindDefinition{AgentKind::ClaudeCode, "claude-code", "Claude Code", "Claude", "claude",
                        "https://claude.com/claude-code"},
    AgentKindDefinition{AgentKind::Codex, "codex", "Codex", "Codex", "codex", "https://github.com/openai/codex"},
    AgentKindDefinition{AgentKind::Custom, "custom", "Custom command", "Agent", "", ""}};

// Saved for the user, not the project: which agent runs, and whether tools may change the editor (`ctl`).
struct AgentPreferences {
    AgentKind kind = AgentKind::ClaudeCode;
    std::string customCommand;
    // Off: the control endpoint refuses commands that change the editor; observing commands still answer.
    bool control = true;
    bool configured = false; // The developer accepted the first-run setup.
    bool operator==(const AgentPreferences&) const = default;
};

[[nodiscard]] constexpr const AgentKindDefinition& AgentKindInfo(AgentKind kind) {
    return AgentKinds[static_cast<std::size_t>(kind)];
}
// The command the agent session runs; empty when Custom has none.
[[nodiscard]] std::string AgentCommand(const AgentPreferences& preferences);
// The program the command starts (its first word, without quotes), for an "is it installed" check.
[[nodiscard]] std::string AgentProgram(std::string_view command);
// The terminal command line: the shell runs the agent and stays open after it exits, so the session never vanishes.
[[nodiscard]] std::string AgentCommandLine(std::string_view shell, std::string_view command);
