#pragma once
#include <array>
#include <set>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

class OutputPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences);

private:
    std::array<char, 256> _search{};
    std::set<std::uint64_t> _selected;
    bool _initialized = false;
};
