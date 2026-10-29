#pragma once
#include <imgui.h>
#include <optional>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

struct EditorShellResult {
    bool quit = false;
    bool toggleMaximize = false;
    bool openCommands = false;
    bool focusCatalog = false;
    bool toggleBottom = false;
    std::optional<EditorPanel> bottomPanel;
    std::optional<EditorWorkspaceLayout> layout;
};

class EditorShell {
public:
    [[nodiscard]] EditorShellResult Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                         EditorPreferences& preferences, int& fontSize, bool maximized) const;
    [[nodiscard]] float Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position,
                                float width) const;
    [[nodiscard]] float Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences, ImVec2 position,
                               float width, EditorShellResult& result) const;
};
