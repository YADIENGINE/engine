#pragma once
#include <imgui.h>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include "Editor/Ui/Panels/Console/ConsoleViews.h"

class TerminalSession;

// What the drawer asks of the workspace after a draw; it never starts, stops or sends to a session itself.
struct AgentsDrawerRequest {
    std::optional<std::uint64_t> sendScene;   // Paste the open scene's context into this session, without Enter
    std::optional<std::uint64_t> close;       // Confirmed: end the session's program
    std::optional<std::uint64_t> restart;     // Start the exited program again
    std::optional<std::uint64_t> openConsole; // Show the session in the Console instead
    std::optional<AgentKind> add;             // "New agent": start one of this kind
};

// The Agents drawer behind the toolbar's agent strip (Docs/Editor/Usage.md → Agent workspace, Design D44): a chip per
// agent session, and the chosen one's live terminal or what it did in the editor. It hangs under the strip at the
// workspace's top right, above the panels, so opening it never rearranges them, and stays open until closed. The
// workspace owns the sessions; the drawer keeps only what it shows.
class AgentsDrawer {
public:
    using Terminals = std::function<TerminalSession*(std::uint64_t)>;

    [[nodiscard]] bool IsOpen() const { return _open; }
    // Opens on `session` when given (else the one shown last), and gives its terminal the keyboard.
    void Open(std::optional<std::uint64_t> session = std::nullopt);
    // The strip's press: closes an open drawer, else opens it on `session`.
    void Toggle(std::optional<std::uint64_t> session);
    void Close() { _open = false; }
    // Keys (EditorAgentsKeys): the next or previous chip, chip `index` (from 0), Terminal ↔ Activity, the New agent
    // menu. They act on the chips of the last draw.
    void Step(int delta);
    void Pick(std::size_t index);
    void SwitchView();
    void AskAdd() { _askAdd = true; }
    // The chosen chip's Console session; empty for the outside chip or with no agent.
    [[nodiscard]] std::optional<std::uint64_t> ChosenSession() const;
    // The agent the developer starts usually (preferences): first and highlighted when there is none yet.
    void SetPreferred(AgentKind kind) { _preferred = kind; }
    // The Console session whose terminal the drawer draws (open, on Terminal); the Console shows a note for it instead.
    [[nodiscard]] std::optional<std::uint64_t> Shown(const std::vector<ConsoleSessionInfo>& sessions) const;
    // The keyboard was in the drawer's terminal during the last draw (Escape then belongs to the agent).
    [[nodiscard]] bool TerminalFocused() const { return _terminalFocused; }
    // Draws into the workspace area `position`/`size`, from its top right corner.
    [[nodiscard]] AgentsDrawerRequest Draw(const std::vector<ConsoleSessionInfo>& sessions, const Terminals& terminals,
                                           ConsoleViews& views, const EditorSnapshot& snapshot,
                                           const EditorUiActions& actions, ImVec2 position, ImVec2 size);

private:
    enum class Filter { All, Editor, Tools };
    enum class View { Terminal, Activity };
    bool _open = false;
    bool _focusTerminal = false;
    bool _terminalFocused = false;
    Filter _filter = Filter::All;
    std::optional<std::string> _selected; // A Console session id as text, or "" for agents outside the Console
    View _view = View::Terminal;
    AgentKind _preferred = AgentKind::ClaudeCode;
    std::vector<std::string> _keys; // The chips of the last draw, in order
    bool _askAdd = false;
    float _width = 0; // Logical pixels; 0 until the developer drags the corner
    float _height = 0;
    std::optional<std::uint64_t> _confirmClose;
};
