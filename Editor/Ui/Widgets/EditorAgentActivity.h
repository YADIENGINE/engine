#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"

// One agent session as a dot on the strip.
struct EditorAgentDot {
    SessionState state = SessionState::Idle;
    bool holds = false; // It holds the editor (ControlLease)
};

// What the strip speaks of: the session that waits for the developer, else the one agent requests go to
// (ConsoleSessions::AgentSession); and a dot for every agent session.
struct EditorAgentStatus {
    bool session = false;               // An agent session exists
    std::optional<std::uint64_t> focus; // The session below; the Agents drawer opens on it
    std::string sessionId;              // Its Console session id as text
    std::string title;                  // "Claude", "Codex 2", ...
    SessionState state = SessionState::Idle;
    std::string activity; // Its note or terminal title; may be empty
    bool control = true;
    std::vector<EditorAgentDot> sessions; // Every agent session, in the Console's order
    bool drawerOpen = false;              // The Agents drawer hangs under the strip
};

// One control event row shared by the toolbar strip and the Agents drawer.
void DrawEditorAgentEventRow(const EditorControlEvent& event, const EditorSnapshot& snapshot,
                             const EditorUiActions& actions, bool highlighted = false);

// Logical widths the toolbar picks between: below Medium only the mark and the count show.
struct EditorAgentActivityMetrics {
    static constexpr float Ideal = 360;
    static constexpr float Medium = 176;
    static constexpr float Compact = 76;
};

// The toolbar's agent strip: the session's state in color, its latest activity and how many came before; with two or
// more agent sessions also a dot per session and the session's name. A press toggles the Agents drawer.
class EditorAgentActivity {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorAgentStatus& agent,
              float width);
    void Show() { _showRequested = true; }
    [[nodiscard]] bool TakeOpenAgents();

private:
    SessionState _state = SessionState::Idle;
    double _doneAt = 0;
    double _flashAt = -1000; // Latest new activity, in ImGui time
    std::size_t _known = 0;  // Activities that have flashed
    std::size_t _seen = 0;   // Activities the developer has had on screen
    bool _showRequested = false;
    bool _openAgents = false;
};
