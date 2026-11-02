#pragma once
#include <array>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

class CatalogPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences);

private:
    void Entry(const SceneDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
               EditorPreferences& preferences);
    std::array<char, 256> _search{};
    int _view = 0;
    std::string _revealedScene; // A newly selected scene opens its category once; collapsing it afterwards sticks
    bool _reveal = false;
};
