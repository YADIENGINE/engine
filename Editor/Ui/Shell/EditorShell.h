#pragma once
#include <imgui.h>
#include <optional>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Agent/SessionActivity.h"
#include "Editor/Internals/Settings/EditorPreferences.h"
#include "Editor/Ui/Widgets/EditorGitBar.h"
#include "Editor/Ui/Widgets/EditorPlaybackControls.h"

// Agent menu / palette / toolbar requests (Docs/Editor/Usage.md → Agent workspace).
enum class EditorAgentRequest { Open, NewShell, SendScene, SendBuildFailure, FixBuild };

struct EditorShellResult {
    bool quit = false;
    bool returnToProjectList = false;
    bool toggleMaximize = false;
    bool openCommands = false; // The palette with commands only
    bool openQuick = false;    // The palette over everything
    bool showProblems = false;
    bool showActivity = false; // The agent strip's list
    bool toggleBottom = false;
    std::optional<EditorPanel> bottomPanel;
    std::optional<EditorWorkspaceLayout> layout;
    std::optional<EditorAgentRequest> agent;
    EditorGitBarResult git;
};

class EditorShell {
public:
    [[nodiscard]] EditorShellResult Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                         EditorPreferences& preferences, int& fontSize, bool maximized,
                                         bool offerProjectList, const std::string& projectName,
                                         EditorGitBar& gitBar) const;
    [[nodiscard]] float Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position,
                                float width, bool& marking, EditorToolbarSearch& search, const EditorAgentStatus& agent,
                                EditorAgentActivity& activity) const;
    [[nodiscard]] float Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences,
                               const EditorAgentStatus& agent, ImVec2 position, float width,
                               EditorShellResult& result) const;
};
