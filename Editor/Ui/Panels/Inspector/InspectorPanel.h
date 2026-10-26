#pragma once
#include "Editor/Application/EditorPresentation.h"

class InspectorPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) const;
};
