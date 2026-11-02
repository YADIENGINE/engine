#pragma once
#include "Editor/Application/EditorPresentation.h"

// What agent tools did through the control endpoint this session, newest first. The viewport's notice fades in seconds;
// this stays, with Undo on a changed parameter and Open on a screenshot.
class ActivityPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);
};
