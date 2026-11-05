#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include "Terminal/TerminalView.h"

// The session list's add button: a shell, or an agent CLI of a kind.
struct ConsoleNewSession {
    std::optional<AgentKind> agent;
};

// The Console panel: the selected session's terminal beside the list of ConsoleSessions (Docs/Editor/Usage.md →
// Agent workspace). It keeps only what drawing needs (each terminal's view, the list's width); the sessions live on
// while the panel is hidden. The first draw opens a shell when there is none.
class ConsolePanel {
public:
    // A request from the add button; the workspace starts it (agent setup may come first).
    [[nodiscard]] std::optional<ConsoleNewSession> Draw(ConsoleSessions& sessions);
    // The keyboard was in a terminal during the last draw.
    [[nodiscard]] bool Focused() const { return _focused; }

private:
    // Returns a close request; the caller keeps running-agent confirmation outside the list's child window.
    [[nodiscard]] bool DrawSessionRow(ConsoleSessions& sessions, const ConsoleSessionInfo& info);
    [[nodiscard]] TerminalView& ViewOf(std::uint64_t id);
    std::map<std::uint64_t, std::unique_ptr<TerminalView>> _views; // By session id
    float _sessionListWidth = 0; // Logical pixels; initialized from the font on first draw
    bool _focused = false;
    std::optional<std::uint64_t> _confirmClose;
};
