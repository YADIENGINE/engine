#pragma once
#include <array>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Git/GitAttention.h"

enum class GitDrawerTab { Changes, Incoming };

struct EditorGitBarResult {
    std::optional<GitDrawerTab> toggle; // Opens that tab, or closes the drawer when it already shows
    std::optional<GitDrawerTab> show;   // Opens that tab
    bool fixWithAgent = false;
};

// Source control's permanent place: the menu bar's far end. It draws from the right edge, gives up its detail as the
// window narrows, and never moves or closes. Docs/Design/README.md, D25.
class EditorGitBar {
public:
    // Inside the main menu bar, after the menus.
    [[nodiscard]] EditorGitBarResult Draw(const GitSnapshot& git, const EditorUiActions& actions,
                                          const std::string& projectName, bool building);

private:
    void Repositories(const GitSnapshot& git, const GitRepositoryState& active, const EditorUiActions& actions,
                      bool building);
    GitAttention _attention;
    std::string _lastProblem;
    std::string _lastNotice;
    double _noticeTime = -1000;
    std::array<char, 128> _newBranch{};
};
