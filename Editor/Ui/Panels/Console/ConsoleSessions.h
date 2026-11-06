#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Internals/Agent/AgentReport.h"
#include "Editor/Internals/Agent/SessionActivity.h"
#include "Terminal/TerminalSession.h"

// One Console session as panels show it (the Console's list, the agent strip, the Agents panel).
struct ConsoleSessionInfo {
    std::uint64_t id = 0;           // Stable for the session's life; its ENGINE_EDITOR_SESSION is this as text
    std::string title;              // "Shell", "Claude 2", ...
    std::optional<AgentKind> agent; // Empty for a shell
    SessionState state = SessionState::Idle;
    std::string activity; // The agent's note or the program's terminal title; may be empty
    bool selected = false;
    bool target = false;                           // Agent requests go here (ConsoleSessions::AgentSession)
    std::chrono::steady_clock::time_point started; // Its program started, or the agent CLI it runs now appeared
    std::optional<std::chrono::steady_clock::time_point> lastOutput; // Since `started`
};

// A read-only preview of an existing routing rule; resolving one never selects or starts a session.
struct ConsoleContextTarget {
    std::optional<std::uint64_t> session;
    std::string title;
    std::string unavailable;
};
[[nodiscard]] ConsoleContextTarget AgentContextTarget(std::span<const ConsoleSessionInfo> sessions, AgentKind kind);
[[nodiscard]] ConsoleContextTarget PasteContextTarget(std::span<const ConsoleSessionInfo> sessions);
[[nodiscard]] ConsoleContextTarget SessionContextTarget(std::span<const ConsoleSessionInfo> sessions,
                                                        std::optional<std::uint64_t> id);

// Local terminal delivery, not an acknowledgement that the agent has begun work.
struct ConsoleContextDelivery {
    std::string context;
    std::string target;
    bool submitted = false;
    std::string problem;
};

// The Console's terminal sessions: shells and agent CLIs, each its own program in the project folder
// (Docs/Editor/Usage.md → Agent workspace). It draws nothing: the Console panel shows it and the workspace drives it.
// Every session's program gets ENGINE_EDITOR_SESSION (its id), so its `ctl` requests name it. Sessions keep running
// while no panel shows them and close with the editor. Sessions are addressed by id; an unknown id does nothing.
class ConsoleSessions {
public:
    void SetDirectory(const std::filesystem::path& directory) { _directory = directory; }
    // Environment added to every session (e.g. the editor's control pipe); given before sessions start.
    void SetEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _environment = std::move(environment);
    }
    // A new shell session, selected; returns its id.
    std::uint64_t OpenShell();
    // A new session running an agent CLI, selected; returns its id.
    std::uint64_t AddAgent(AgentKind kind, const std::string& commandLine);
    // Selects the session; an exited program restarts with `commandLine` (empty: its previous one).
    void Revive(std::uint64_t id, const std::string& commandLine);
    // Pastes into the selected session's program (no Enter); opens a shell when there is none.
    void Paste(const std::string& text, const std::string& context = {});
    // Pastes into the session and selects it; `submit` presses Enter after the text. A program that just started gets
    // the text once its output has settled.
    void SendTo(std::uint64_t id, const std::string& text, bool submit, const std::string& context = {});
    [[nodiscard]] std::vector<ConsoleContextDelivery> TakeContextDeliveries();
    void Select(std::uint64_t id);
    // Ends the session's program and removes it.
    void Close(std::uint64_t id);
    // Every frame, also while no panel shows the sessions: processes output, notices bells and which agent CLI runs.
    // `report` (may be null) holds the agents' notes.
    void Update(const EditorAgentReport* report);

    [[nodiscard]] bool Empty() const { return _sessions.empty(); }
    [[nodiscard]] std::vector<ConsoleSessionInfo> Infos() const;
    [[nodiscard]] std::optional<ConsoleSessionInfo> Info(std::uint64_t id) const;
    // The selected session; empty when there is none.
    [[nodiscard]] std::optional<std::uint64_t> Selected() const;
    // Where agent requests go: the selected session if it runs an agent, else the agent session selected last.
    [[nodiscard]] std::optional<std::uint64_t> AgentSession() const;
    // The session's terminal, for the view that draws it; null for an unknown id.
    [[nodiscard]] TerminalSession* Terminal(std::uint64_t id);

private:
    struct Session {
        Session(std::uint64_t sessionId, std::string sessionTitle, std::optional<AgentKind> agentKind);
        std::uint64_t id;
        std::string title;
        std::optional<AgentKind> agent;
        std::optional<AgentKind> launchAgent; // Custom commands retain their user-selected identity.
        bool agentObserved = false;           // The CLI may take a moment to spawn after its shell starts.
        TerminalSession terminal;
        std::string pendingText; // Waits for a starting program (SendTo)
        bool pendingSubmit = false;
        std::vector<std::string> pendingContexts; // Receipts wait for the same terminal write as pendingText.
        std::chrono::steady_clock::time_point started;
        std::optional<std::chrono::steady_clock::time_point> lastOutput;
        std::optional<std::chrono::steady_clock::time_point> lastBell;
        std::uint64_t outputBytes = 0;
    };
    Session& Add(std::string title, std::optional<AgentKind> agent, const std::string& commandLine);
    void Start(Session& session, const std::string& commandLine);
    [[nodiscard]] Session* Find(std::uint64_t id);
    [[nodiscard]] const Session* Find(std::uint64_t id) const;
    [[nodiscard]] std::string NextTitle(const std::string& base) const;
    [[nodiscard]] SessionSignals Signals(const Session& session) const;
    [[nodiscard]] ConsoleSessionInfo InfoOf(const Session& session) const;
    void CompleteContexts(Session& session, const std::string& problem = {});
    std::vector<ConsoleContextDelivery> _contextDeliveries;
    std::vector<std::unique_ptr<Session>> _sessions;
    std::uint64_t _selected = 0; // Id; 0 when there is no session
    std::uint64_t _nextId = 1;
    std::uint64_t _lastAgent = 0; // Id of the agent session selected last
    std::chrono::steady_clock::time_point _nextProcessPoll;
    const EditorAgentReport* _report = nullptr;
    std::filesystem::path _directory;
    std::vector<std::pair<std::string, std::string>> _environment;
};
