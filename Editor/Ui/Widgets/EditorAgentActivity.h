#pragma once
#include <cstddef>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"

// What the editor knows about the agent session agent requests go to (ConsolePanel::AgentSession).
struct EditorAgentStatus {
    bool session = false; // An agent session exists
    std::string title;    // "Claude", "Codex 2", ...
    SessionState state = SessionState::Idle;
    std::string activity; // Its note or terminal title; may be empty
    bool control = true;
};

// Logical widths the toolbar picks between: below Medium only the mark and the count show.
struct EditorAgentActivityMetrics {
    static constexpr float Ideal = 300;
    static constexpr float Medium = 176;
    static constexpr float Compact = 76;
};

// The toolbar's agent strip: the session's state in color, its latest activity and how many came before. A press opens
// what agent tools did this session, newest first, with Undo on a changed parameter and Open on a screenshot.
class EditorAgentActivity {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorAgentStatus& agent,
              float width);
    void Show() { _showRequested = true; }
    // "Open session" was pressed since the last call.
    [[nodiscard]] bool TakeOpenSession();

private:
    void DrawList(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorAgentStatus& agent);

    SessionState _state = SessionState::Idle;
    double _doneAt = 0;
    double _flashAt = -1000;     // Latest new activity, in ImGui time
    std::size_t _known = 0;      // Activities that have flashed
    std::size_t _seen = 0;       // Activities the developer has had on screen
    std::size_t _seenAtOpen = 0; // The list marks what came after this
    bool _showRequested = false;
    bool _openSession = false;
};
