#pragma once
#include <string>
#include "Editor/Internals/Agent/AgentProfile.h"

// The same radio choices and custom command editor in the Agent menu and setup modal.
void EditorAgentChoice(AgentPreferences& preferences, float commandWidth = -1);

enum class EditorAgentSetupResult { None, Started, Canceled };

class EditorAgentSetup {
public:
    void Open(const AgentPreferences& preferences);
    [[nodiscard]] bool Active() const { return _active; }
    [[nodiscard]] EditorAgentSetupResult Draw(AgentPreferences& preferences);

private:
    AgentPreferences _draft;
    std::string _checkedCommand;
    bool _available = false;
    bool _active = false;
    bool _opening = false;
};
