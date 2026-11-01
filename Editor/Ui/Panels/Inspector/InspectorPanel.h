#pragma once
#include <array>
#include <string>
#include <utility>
#include "Editor/Application/EditorPresentation.h"

class InspectorPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] std::string TakeAgentText() { return std::exchange(_agentText, {}); }

private:
    std::array<char, 128> _search{};
    std::string _scene;
    std::string _contextParameter;
    std::string _agentText;
    bool _modifiedOnly = false;
    bool _searchVisible = false;
};
