#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Terminal/TerminalSession.h"
#include "Terminal/TerminalView.h"

struct ConsoleSessionInfo {
    std::string title;
    bool agent = false;
    bool running = false;
    bool attention = false; // Rang the bell since the developer last looked at it (an agent waiting for input)
};

// The bottom dock's terminal sessions: shells in the project folder and at most one agent session that runs the
// developer's agent CLI (Docs/Editor/Usage.md → Agent workspace). Sessions keep running while the panel is hidden and
// close with the editor. The first draw opens a shell when there is none.
class ConsolePanel {
public:
    ConsolePanel() = default;
    void SetDirectory(const std::filesystem::path& directory) { _directory = directory; }
    // Environment added to every session (e.g. the editor's control pipe); given before sessions start.
    void SetEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _environment = std::move(environment);
    }
    // A new shell session, selected.
    void OpenShell();
    // Selects the agent session; starts `commandLine` in it when it has none or its program exited.
    void OpenAgent(const std::string& title, const std::string& commandLine);
    // Pastes into the selected session's program (no Enter); opens a shell when there is none.
    void Paste(const std::string& text);
    // Pastes into the agent session (opening it like OpenAgent) and selects it; `submit` presses Enter after the
    // text. A session that just started gets the text once its program has settled.
    void SendToAgent(const std::string& title, const std::string& commandLine, const std::string& text, bool submit);
    void Select(std::size_t index);
    void Close(std::size_t index);
    // Every frame, also while hidden: processes output and notices bells.
    void Update();
    void Draw();
    // The keyboard was in a terminal during the last draw.
    [[nodiscard]] bool Focused() const;
    [[nodiscard]] std::vector<ConsoleSessionInfo> Sessions() const;
    [[nodiscard]] std::size_t Selected() const { return _selected; }
    [[nodiscard]] std::optional<std::size_t> AgentSession() const;

private:
    struct Session {
        Session(std::string sessionTitle, bool agentSession);
        std::string title;
        bool agent = false;
        bool attention = false;
        TerminalSession terminal;
        TerminalView view;
        std::string pendingText; // Waits for a starting program (SendToAgent)
        bool pendingSubmit = false;
        std::chrono::steady_clock::time_point started;
        std::chrono::steady_clock::time_point lastOutput;
        std::uint64_t outputBytes = 0;
    };
    Session& Add(std::string title, bool agent, const std::string& commandLine);
    void Start(Session& session, const std::string& commandLine);
    std::vector<std::unique_ptr<Session>> _sessions;
    std::size_t _selected = 0;
    bool _selectTab = false; // The selection changed outside the tab bar
    bool _focused = false;
    std::filesystem::path _directory;
    std::vector<std::pair<std::string, std::string>> _environment;
};
