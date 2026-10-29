#pragma once
#include <array>
#include <set>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Settings/EditorPreferences.h"

class OutputPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions, EditorPreferences& preferences);
    void ShowLastAttempt(std::uint64_t attempt);

private:
    std::array<char, 256> _search{};
    std::set<std::uint64_t> _selected;
    std::uint64_t _anchor = 0;
    std::uint64_t _attemptSeen = 0;
    std::vector<char> _detail;
    EditorLogFilter _failureFilter;
    bool _lastAttempt = true;
    bool _failureView = false;
    bool _jumpToError = false;
    bool _initialized = false;
};
