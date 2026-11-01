#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Internals/Agent/SessionActivity.h"
#include "Terminal/TerminalSession.h"
#include "Terminal/TerminalView.h"

struct ConsoleSessionInfo {
    std::string title;              // "Shell", "Claude 2", ...
    std::optional<AgentKind> agent; // Empty for a shell
    SessionState state = SessionState::Idle;
    std::string activity; // Hover text: the agent's note or the program's terminal title; may be empty
    bool selected = false;
};

// The session list's add button: a shell, or an agent CLI of a kind.
struct ConsoleNewSession {
    std::optional<AgentKind> agent;
};

// An agent's latest `ctl note` with the time it arrived, by session id.
struct ConsoleNote {
    AgentNote note;
    std::chrono::steady_clock::time_point time;
};

// The Console's terminal sessions: shells and agent CLIs, each its own program in the project folder
// (Docs/Editor/Usage.md → Agent workspace). Every session's program gets ENGINE_EDITOR_SESSION (its id), so its
// `ctl note` reaches its own row. Sessions keep running while the panel is hidden and close with the editor. The
// first draw opens a shell when there is none.
class ConsolePanel {
public:
    ConsolePanel() = default;
    void SetDirectory(const std::filesystem::path& directory) { _directory = directory; }
    // Environment added to every session (e.g. the editor's control pipe); given before sessions start.
    void SetEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _environment = std::move(environment);
    }
    // The agents' notes (EditorControlService::SessionActivity), set every frame.
    void SetNotes(std::map<std::string, ConsoleNote> notes) { _notes = std::move(notes); }
    // A new shell session, selected.
    void OpenShell();
    // A new session running an agent CLI, selected.
    void AddAgent(AgentKind kind, const std::string& commandLine);
    // Selects session `index`; an exited program restarts with `commandLine` (empty: its previous one).
    void Revive(std::size_t index, const std::string& commandLine);
    // Pastes into the selected session's program (no Enter); opens a shell when there is none.
    void Paste(const std::string& text);
    // Pastes into session `index` and selects it; `submit` presses Enter after the text. A program that just started
    // gets the text once its output has settled.
    void SendTo(std::size_t index, const std::string& text, bool submit);
    void Select(std::size_t index);
    void Close(std::size_t index);
    // Every frame, also while hidden: processes output and notices bells.
    void Update();
    // A request from the add button; the workspace starts it (agent setup may come first).
    [[nodiscard]] std::optional<ConsoleNewSession> Draw();
    // The keyboard was in a terminal during the last draw.
    [[nodiscard]] bool Focused() const;
    [[nodiscard]] std::vector<ConsoleSessionInfo> Sessions() const;
    [[nodiscard]] std::size_t Selected() const { return _selected; }
    // Where agent requests go: the selected session if it runs an agent, else the agent session selected last.
    [[nodiscard]] std::optional<std::size_t> AgentSession() const;

private:
    struct Session {
        Session(std::uint64_t sessionId, std::string sessionTitle, std::optional<AgentKind> agentKind);
        std::uint64_t id;
        std::string title;
        std::optional<AgentKind> agent;
        TerminalSession terminal;
        TerminalView view;
        std::string pendingText; // Waits for a starting program (SendTo)
        bool pendingSubmit = false;
        std::chrono::steady_clock::time_point started;
        std::optional<std::chrono::steady_clock::time_point> lastOutput;
        std::optional<std::chrono::steady_clock::time_point> lastBell;
        std::uint64_t outputBytes = 0;
    };
    Session& Add(std::string title, std::optional<AgentKind> agent, const std::string& commandLine);
    void Start(Session& session, const std::string& commandLine);
    [[nodiscard]] std::string NextTitle(const std::string& base) const;
    [[nodiscard]] SessionSignals Signals(const Session& session) const;
    // Returns a close request; the caller keeps running-agent confirmation outside the list's child window.
    [[nodiscard]] bool DrawSessionRow(std::size_t index, const ConsoleSessionInfo& info);
    std::vector<std::unique_ptr<Session>> _sessions;
    std::size_t _selected = 0;
    std::uint64_t _nextId = 1;
    std::uint64_t _lastAgent = 0; // Id of the agent session selected last
    float _sessionListWidth = 0;  // Logical pixels; initialized from the font on first draw
    bool _focused = false;
    std::optional<std::size_t> _confirmClose;
    std::map<std::string, ConsoleNote> _notes;
    std::filesystem::path _directory;
    std::vector<std::pair<std::string, std::string>> _environment;
};
