#pragma once
#include <imgui.h>
#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Ui/Widgets/EditorGitBar.h"

// The list behind the Git bar: what changed, what is coming, and whatever needs a decision. Fixed to the workspace's
// left edge above the panels, so opening it never rearranges them or shrinks the scene. Docs/Design/README.md, D25.
class GitDrawer {
public:
    [[nodiscard]] bool IsOpen() const { return _open; }
    void Show(GitDrawerTab tab) {
        _open = true;
        _tab = tab;
    }
    void Toggle(GitDrawerTab tab);
    void Close() { _open = false; }
    // The right edge of what it covers; the diff view starts there.
    void Draw(const GitSnapshot& git, const EditorUiActions& actions, ImVec2 position, ImVec2 size, bool building);
    // Context for "Fix with agent"; empty when not requested.
    [[nodiscard]] std::string TakeAgentText() { return std::exchange(_agentText, {}); }

private:
    struct Row {
        std::string folder; // A folder heading when `change` is null
        const GitChange* change = nullptr;
    };
    void Problem(const GitSnapshot& git, const EditorUiActions& actions);
    void Changes(const GitSnapshot& git, const GitRepositoryState& repository, const EditorUiActions& actions);
    void Conflicts(const GitRepositoryState& repository, const EditorUiActions& actions, bool busy);
    void CommitForm(const GitSnapshot& git, const GitRepositoryState& repository, const EditorUiActions& actions);
    void Incoming(const GitSnapshot& git, const GitRepositoryState& repository, const EditorUiActions& actions,
                  bool building);
    void IdentityForm(const EditorUiActions& actions);
    void RemoteForm(const EditorUiActions& actions);
    void SendCommit(const GitRepositoryState& repository, const EditorUiActions& actions, bool thenPush);
    bool _open = false;
    GitDrawerTab _tab = GitDrawerTab::Changes;
    std::set<std::string> _selected; // Nothing is selected to begin with: only what the developer chose gets committed
    std::array<char, 4096> _message{};
    std::array<char, 128> _name{};
    std::array<char, 128> _email{};
    std::array<char, 512> _remote{};
    std::uint64_t _committed = 0;
    std::optional<GitRequest> _lastCommit; // "Commit anyway" repeats it
    std::string _agentText;
};
