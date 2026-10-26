#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

struct EditorShellResult {
    bool quit = false;
    bool restoreLayout = false;
    bool toggleMaximize = false;
};

class EditorShell {
public:
    [[nodiscard]] EditorShellResult Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                         EditorPreferences& preferences, int& fontSize, bool maximized) const;
    [[nodiscard]] float Status(const EditorSnapshot& snapshot, ImVec2 position, float width) const;
};
