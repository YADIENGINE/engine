#pragma once
#include <imgui.h>
#include <optional>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

// Agent menu / palette / status bar requests (Docs/Editor/Usage.md → Agent workspace).
enum class EditorAgentRequest { Open, NewShell, SendScene, SendBuildFailure, FixBuild };

// What the status bar shows about the agent session agent requests go to (ConsolePanel::AgentSession).
struct EditorAgentStatus {
    bool session = false; // An agent session exists
    std::string title;    // "Claude", "Codex 2", ...
    SessionState state = SessionState::Idle;
    std::string activity; // Its note or terminal title; may be empty
    bool control = true;
};

struct EditorShellResult {
    bool quit = false;
    bool returnToProjectList = false;
    bool toggleMaximize = false;
    bool openCommands = false;
    bool focusCatalog = false;
    bool toggleBottom = false;
    std::optional<EditorPanel> bottomPanel;
    std::optional<EditorWorkspaceLayout> layout;
    std::optional<EditorAgentRequest> agent;
};

class EditorShell {
public:
    [[nodiscard]] EditorShellResult Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                         EditorPreferences& preferences, int& fontSize, bool maximized,
                                         bool offerProjectList) const;
    [[nodiscard]] float Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position,
                                float width, bool& marking) const;
    [[nodiscard]] float Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences,
                               const EditorAgentStatus& agent, ImVec2 position, float width,
                               EditorShellResult& result) const;
};
