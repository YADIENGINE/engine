#pragma once
#include <cstdint>
#include <optional>
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include "Editor/Ui/Panels/Console/ConsoleViews.h"

// The session list's add button: a shell, or an agent CLI of a kind.
struct ConsoleNewSession {
    std::optional<AgentKind> agent;
};
// What the Console asks of the workspace after a draw.
struct ConsoleRequest {
    std::optional<ConsoleNewSession> add; // The add button; the workspace starts it (agent setup may come first)
    bool showHere = false;                // "Show here" on a session the Agents drawer shows: close the drawer
};

// Asks before a running agent's session closes; open it with ImGui::OpenPopup(CloseAgentDialogId) in the same window.
// Returns the session to close once confirmed; `pending` is the one asked about and clears when answered.
inline constexpr const char* CloseAgentDialogId = "Close agent session?";
[[nodiscard]] std::optional<std::uint64_t> DrawCloseAgentDialog(std::optional<std::uint64_t>& pending);

// The Console panel: the selected session's terminal beside the list of ConsoleSessions (Docs/Editor/Usage.md →
// Agent workspace). It keeps only the list's width and its close confirmation; the sessions and their views live on
// while the panel is hidden. The first draw opens a shell when there is none.
class ConsolePanel {
public:
    // `elsewhere`: the session whose terminal the Agents drawer draws this frame; the Console shows a note instead.
    [[nodiscard]] ConsoleRequest Draw(ConsoleSessions& sessions, ConsoleViews& views,
                                      std::optional<std::uint64_t> elsewhere = std::nullopt);
    // The keyboard was in a terminal during the last draw.
    [[nodiscard]] bool Focused() const { return _focused; }

private:
    // Returns a close request; the caller keeps running-agent confirmation outside the list's child window.
    [[nodiscard]] bool DrawSessionRow(ConsoleSessions& sessions, const ConsoleSessionInfo& info);
    float _sessionListWidth = 0; // Logical pixels; initialized from the font on first draw
    bool _focused = false;
    std::optional<std::uint64_t> _confirmClose;
};
