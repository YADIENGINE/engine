#pragma once
#include <array>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

class CatalogPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences);
    void FocusSearch() { _focusSearch = true; }

private:
    void Entry(const PreviewDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
               EditorPreferences& preferences);
    std::array<char, 256> _search{};
    int _view = 0;
    bool _focusSearch = false;
};
