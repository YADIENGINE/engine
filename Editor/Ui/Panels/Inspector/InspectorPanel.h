#pragma once
#include <array>
#include <string>
#include "Editor/Application/EditorPresentation.h"

class InspectorPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    std::array<char, 128> _search{};
    std::string _preview;
    bool _modifiedOnly = false;
    bool _searchVisible = false;
};
