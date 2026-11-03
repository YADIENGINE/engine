#pragma once
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"

// One file's change, drawn over the scene's place: a diff wants the room the scene has. Pictures show before and after
// side by side and sounds play either way, because a text diff says nothing about either.
class GitDiffView {
public:
    void Draw(const GitDiff& diff, const EditorUiActions& actions, ImVec2 position, ImVec2 size) const;
};
