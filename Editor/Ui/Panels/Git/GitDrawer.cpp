#include "Editor/Ui/Panels/Git/GitDrawer.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>
#include <format>
#include "Editor/Internals/Git/GitParsing.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float Padding = 12.0f;
constexpr float MessageLines = 3.5f;
constexpr float RowPadding = 2.0f;
constexpr int MaximumProblemFiles = 8;
constexpr ImGuiWindowFlags DrawerFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr const char* GetLfsUrl = "https://git-lfs.com";

void Send(const EditorUiActions& actions, GitRequest request) {
    if (actions.git) actions.git(request);
}
void Send(const EditorUiActions& actions, GitRequestKind kind) { Send(actions, GitRequest{.kind = kind}); }
std::string Folder(const std::string& path) {
    const auto trimmed = path.ends_with('/') ? path.substr(0, path.size() - 1) : path;
    const auto slash = trimmed.rfind('/');
    return slash == std::string::npos ? std::string{} : trimmed.substr(0, slash);
}
std::string FileName(const std::string& path) {
    const auto trimmed = path.ends_with('/') ? path.substr(0, path.size() - 1) : path;
    const auto slash = trimmed.rfind('/');
    return (slash == std::string::npos ? trimmed : trimmed.substr(slash + 1)) + (path.ends_with('/') ? "/" : "");
}
const char* KindLetter(const GitChange& change) {
    switch (change.kind) {
    case GitChangeKind::Modified   : return "M";
    case GitChangeKind::Added      : return "A";
    case GitChangeKind::Deleted    : return "D";
    case GitChangeKind::Renamed    : return "R";
    case GitChangeKind::Copied     : return "C";
    case GitChangeKind::TypeChanged: return "T";
    case GitChangeKind::Untracked  : return "U";
    case GitChangeKind::Conflicted : return "!";
    }
    return "";
}
ImVec4 KindColor(const GitChange& change) {
    switch (change.kind) {
    case GitChangeKind::Added:
    case GitChangeKind::Untracked : return ToolToneColor(ToolTone::Success);
    case GitChangeKind::Deleted   :
    case GitChangeKind::Conflicted: return ToolToneColor(ToolTone::Error);
    default                       : return ToolPalette::Accent;
    }
}
std::string TimeAgo(std::int64_t time) {
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto seconds = std::max<std::int64_t>(now - time, 0);
    if (seconds < 3600) return std::format("{} min ago", seconds / 60);
    if (seconds < 86400) return std::format("{} h ago", seconds / 3600);
    return std::format("{} d ago", seconds / 86400);
}
// A tick that lands on the row's left. `mixed`: some of a folder's files.
bool Tick(const char* id, bool on, bool mixed) {
    const float side = ImGui::GetFontSize() * 0.8f;
    const auto start = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight();
    const bool pressed = ImGui::InvisibleButton(id, {side + 4 * ToolUiScale(), height});
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 from{start.x, start.y + (height - side) * 0.5f};
    const ImVec2 to{from.x + side, from.y + side};
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(from, to, ImGui::GetColorU32(hovered ? ToolPalette::Hover : ToolPalette::Control),
                        ToolMetrics::Radius * ToolUiScale());
    if (on) {
        const float inset = side * 0.22f;
        draw->AddRectFilled({from.x + inset, from.y + inset}, {to.x - inset, to.y - inset},
                            ImGui::GetColorU32(ToolPalette::Accent));
    } else if (mixed) {
        const float inset = side * 0.22f;
        const float middle = (from.y + to.y) * 0.5f;
        draw->AddRectFilled({from.x + inset, middle - 1 * ToolUiScale()}, {to.x - inset, middle + 1 * ToolUiScale()},
                            ImGui::GetColorU32(ToolPalette::Accent));
    }
    return pressed;
}
void Note(const char* text, ImVec4 color) {
    ImGui::PushTextWrapPos(0);
    ImGui::TextColored(color, "%s", text);
    ImGui::PopTextWrapPos();
}
void AgentButtonStart() { PushEditorTintedButton(EditorAgentColor(), EditorAgentColor()); }
// The drawer's main action: a tint, so the Inspector's primary button stays the screen's one strong amber (D23).
bool MainButton(const char* label, ImVec2 size) {
    const bool enabled = !(ImGui::GetItemFlags() & ImGuiItemFlags_Disabled);
    if (enabled) PushEditorTintedButton(ToolPalette::Accent, ToolPalette::Accent);
    const bool pressed = ToolButton(label, size);
    if (enabled) PopEditorButton();
    return pressed;
}
std::string ConflictPrompt(const GitRepositoryState& repository) {
    const auto folder = repository.directory.generic_u8string();
    std::string text = "Resolve these git conflicts in " + std::string(reinterpret_cast<const char*>(folder.c_str())) +
                       ". Keep the intent of both sides; when the sides truly exclude each other, ask me. "
                       "Stage each file with `git add` when it is resolved. Do not commit.\n";
    for (const auto& change : repository.changes)
        if (change.kind == GitChangeKind::Conflicted) text += "- " + change.path + "\n";
    return text;
}
void AddPaths(std::vector<std::string>& paths, const GitChange& change) {
    paths.push_back(change.path);
    if (!change.originalPath.empty()) paths.push_back(change.originalPath); // The rename's other half
}
} // namespace

void GitDrawer::Toggle(GitDrawerTab tab) {
    _open = !_open || _tab != tab;
    _tab = tab;
}

void GitDrawer::IdentityForm(const EditorUiActions& actions) {
    Note("Git records a name and email with every commit.", ToolPalette::Secondary);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##git-name", "Your name", _name.data(), _name.size());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##git-email", "Email", _email.data(), _email.size());
    ImGui::BeginDisabled(_name[0] == '\0' || _email[0] == '\0');
    if (ToolButton("Save for all projects"))
        Send(actions,
             GitRequest{.kind = GitRequestKind::SetIdentity, .text = _name.data(), .secondText = _email.data()});
    ImGui::EndDisabled();
    ImGui::Spacing();
}

void GitDrawer::RemoteForm(const EditorUiActions& actions) {
    Note("This project is only on this computer. Create an empty repository on GitHub or another host, then paste its "
         "address.",
         ToolPalette::Secondary);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##git-remote", "https://github.com/you/project.git", _remote.data(), _remote.size());
    ImGui::BeginDisabled(_remote[0] == '\0');
    if (ToolButton("Publish")) Send(actions, GitRequest{.kind = GitRequestKind::AddRemote, .text = _remote.data()});
    ImGui::EndDisabled();
    ImGui::Spacing();
}

void GitDrawer::Problem(const GitSnapshot& git, const EditorUiActions& actions) {
    const auto& problem = git.problem;
    if (problem.kind == GitProblemKind::None) return;
    const auto tone = problem.kind == GitProblemKind::Failed || problem.kind == GitProblemKind::Conflicts
                          ? ToolToneColor(ToolTone::Error)
                          : ToolPalette::Accent;
    Note(problem.title.c_str(), tone);
    for (int index = 0; index < static_cast<int>(problem.files.size()); ++index) {
        if (index == MaximumProblemFiles) {
            ImGui::TextColored(ToolPalette::Muted, "... and %d more", static_cast<int>(problem.files.size()) - index);
            break;
        }
        ImGui::TextColored(
            ToolPalette::Secondary, "%s",
            ToolClipMiddle(problem.files[static_cast<std::size_t>(index)].c_str(), ImGui::GetContentRegionAvail().x)
                .c_str());
    }
    if (problem.kind == GitProblemKind::Failed && !problem.detail.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::Muted);
        ImGui::PushTextWrapPos(0);
        const auto lines = SplitGitLines(problem.detail);
        ImGui::TextUnformatted(lines.empty() ? "" : lines.front().c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }
    const bool busy = !git.busy.empty();
    ImGui::BeginDisabled(busy);
    bool sameLine = false;
    const auto button = [&](const char* label) {
        if (std::exchange(sameLine, true)) ImGui::SameLine();
        return ToolButton(label);
    };
    switch (problem.kind) {
    case GitProblemKind::LocalChanges:
        if (button("Stash & Pull")) Send(actions, GitRequestKind::StashAndPull);
        ToolTooltip("Saves your changes in the stash, pulls, and puts them back.");
        break;
    case GitProblemKind::PushRejected:
    case GitProblemKind::Diverged:
        if (button("See incoming")) _tab = GitDrawerTab::Incoming;
        break;
    case GitProblemKind::Authentication:
    case GitProblemKind::Offline:
        if (button("Try again")) Send(actions, GitRequestKind::Fetch);
        break;
    case GitProblemKind::Locked:
        if (button("Remove lock")) Send(actions, GitRequestKind::RemoveLock);
        ToolTooltip("Only if no other git is running: a crash can leave this file behind.");
        break;
    case GitProblemKind::LargeFiles:
        if (_lastCommit && button("Commit anyway")) {
            auto again = *_lastCommit;
            again.allowLarge = true;
            Send(actions, std::move(again));
        }
        break;
    case GitProblemKind::UnpushedSubmodule:
        if (button("Push them too")) Send(actions, GitRequestKind::PushSubmodules);
        break;
    case GitProblemKind::SubmoduleNotUpdated:
        if (button("Try again")) Send(actions, GitRequestKind::UpdateSubmodules);
        ToolTooltip(problem.detail.c_str());
        break;
    case GitProblemKind::LfsMissing:
        if (button("Get Git LFS") && actions.openPath) actions.openPath(GetLfsUrl);
        break;
    default: break;
    }
    ImGui::EndDisabled();
    if (button("Dismiss")) Send(actions, GitRequestKind::Dismiss);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void GitDrawer::Conflicts(const GitRepositoryState& repository, const EditorUiActions& actions, bool busy) {
    Note("These files changed on both sides. Choose one side, or edit the file and mark it resolved.",
         ToolPalette::Secondary);
    AgentButtonStart();
    if (ToolButton("Fix with agent")) _agentText = ConflictPrompt(repository);
    PopEditorButton();
    ToolTooltip("Sends the list to the agent session and asks it to resolve them.");
    ImGui::Spacing();
    ImGui::BeginDisabled(busy);
    for (const auto& change : repository.changes) {
        if (change.kind != GitChangeKind::Conflicted) continue;
        ImGui::PushID(change.path.c_str());
        ImGui::TextColored(ToolToneColor(ToolTone::Error), "%s", KindLetter(change));
        ImGui::SameLine();
        ImGui::TextUnformatted(ToolClipMiddle(change.path.c_str(), ImGui::GetContentRegionAvail().x).c_str());
        ToolTooltip(change.path.c_str());
        const auto side = [&](GitConflictSide choice) {
            Send(actions, GitRequest{.kind = GitRequestKind::TakeConflictSide, .paths = {change.path}, .side = choice});
        };
        if (ToolButton("Mine")) side(GitConflictSide::Mine);
        ToolTooltip("Keep your version of this file.");
        ImGui::SameLine();
        if (ToolButton("Theirs")) side(GitConflictSide::Theirs);
        ToolTooltip("Take the incoming version of this file.");
        ImGui::SameLine();
        if (ToolButton("Resolved"))
            Send(actions, GitRequest{.kind = GitRequestKind::MarkResolved, .paths = {change.path}});
        ToolTooltip("You edited the file by hand and removed the conflict markers.");
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    ImGui::Spacing();
}

void GitDrawer::SendCommit(const GitRepositoryState& repository, const EditorUiActions& actions, bool thenPush) {
    GitRequest request{.kind = GitRequestKind::Commit, .text = _message.data(), .thenPush = thenPush};
    for (const auto& change : repository.changes)
        if (_selected.contains(change.path)) AddPaths(request.paths, change);
    _lastCommit = request;
    Send(actions, std::move(request));
}

void GitDrawer::CommitForm(const GitSnapshot& git, const GitRepositoryState& repository,
                           const EditorUiActions& actions) {
    const bool merging = repository.sequence == GitSequence::Merge;
    const auto conflicts = std::ranges::count(repository.changes, GitChangeKind::Conflicted, &GitChange::kind);
    const bool detached = repository.branch.head.empty();
    if (repository.userName.empty() || repository.userEmail.empty()) {
        IdentityForm(actions);
        return;
    }
    if (detached) {
        Note("Not on a branch. Choose a branch from the bar before committing, or the commit is easy to lose.",
             ToolPalette::Accent);
        return;
    }
    const auto count = _selected.size();
    ImGui::InputTextMultiline("##git-message", _message.data(), _message.size(),
                              {-1, ImGui::GetTextLineHeightWithSpacing() * MessageLines});
    if (_message[0] == '\0' && !ImGui::IsItemActive()) {
        const auto start = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(
            {start.x + ImGui::GetStyle().FramePadding.x, start.y + ImGui::GetStyle().FramePadding.y},
            ImGui::GetColorU32(ToolPalette::Muted), merging ? "Merge message (optional)" : "What changed, and why");
    }
    const bool hasMessage = _message[0] != '\0';
    const bool can = git.busy.empty() && conflicts == 0 && (merging || (count > 0 && hasMessage));
    const float width = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const auto commitLabel =
        merging ? std::string("Complete merge") : std::format("Commit {} of {}", count, repository.changes.size());
    ImGui::BeginDisabled(!can);
    const bool hasRemote = !repository.remotes.empty();
    const float commitWidth = hasRemote ? (width - gap) * 0.5f : width;
    if (MainButton((commitLabel + "###git-commit-now").c_str(), {commitWidth, 0}))
        SendCommit(repository, actions, false);
    if (hasRemote) {
        ImGui::SameLine();
        if (ToolButton("Commit & Push", {commitWidth, 0})) SendCommit(repository, actions, true);
    }
    ImGui::EndDisabled();
    if (!can && git.busy.empty()) {
        ToolTooltip(conflicts > 0            ? "Resolve the conflicts first."
                    : count == 0 && !merging ? "Tick the files to commit."
                                             : "Write what changed.");
    }
}

void GitDrawer::Changes(const GitSnapshot& git, const GitRepositoryState& repository, const EditorUiActions& actions) {
    const bool busy = !git.busy.empty();
    if (repository.sequence != GitSequence::None) {
        const char* what =
            repository.sequence == GitSequence::Merge        ? "A merge is in progress."
            : repository.sequence == GitSequence::Rebase     ? "A rebase is in progress. Finish it in the Console."
            : repository.sequence == GitSequence::CherryPick ? "A cherry-pick is in progress. Finish it in the Console."
            : repository.sequence == GitSequence::Revert     ? "A revert is in progress. Finish it in the Console."
                                                             : "A bisect is in progress. Finish it in the Console.";
        Note(what, ToolPalette::Accent);
        if (repository.sequence != GitSequence::Bisect) {
            ImGui::BeginDisabled(busy);
            if (ToolButton("Cancel it")) Send(actions, GitRequestKind::AbortSequence);
            ImGui::EndDisabled();
            ToolTooltip("Puts the files back as they were before it started.");
        }
        ImGui::Spacing();
    }
    if (repository.remotes.empty() && !repository.branch.oid.empty()) RemoteForm(actions);
    const auto conflicts = std::ranges::count(repository.changes, GitChangeKind::Conflicted, &GitChange::kind);
    if (conflicts > 0) Conflicts(repository, actions, busy);

    // Selections of files that are gone (committed, reverted by someone) drop out.
    std::erase_if(_selected, [&](const std::string& path) {
        return std::ranges::find(repository.changes, path, &GitChange::path) == repository.changes.end();
    });
    const bool merging = repository.sequence == GitSequence::Merge;
    std::vector<const GitChange*> changes;
    changes.reserve(repository.changes.size());
    for (const auto& change : repository.changes)
        if (change.kind != GitChangeKind::Conflicted) changes.push_back(&change);
    std::ranges::stable_sort(changes, [](const GitChange* left, const GitChange* right) {
        return Folder(left->path) < Folder(right->path);
    });

    if (changes.empty() && conflicts == 0) {
        ToolEmptyState("Nothing to commit", "Files you change appear here.");
    } else if (!changes.empty()) {
        if (!merging) {
            const bool all = _selected.size() == repository.changes.size();
            if (Tick("##git-all", all, !_selected.empty())) {
                _selected.clear();
                if (!all)
                    for (const auto* change : changes)
                        _selected.insert(change->path);
            }
            ToolTooltip("Select all. Check the list first when an agent or a teammate shares this folder.");
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ToolPalette::Muted, "%zu of %zu selected", _selected.size(), repository.changes.size());
        }
        std::vector<Row> rows;
        rows.reserve(changes.size() + 8);
        std::optional<std::string> folder;
        for (const auto* change : changes) {
            auto own = Folder(change->path);
            if (!folder || *folder != own) {
                rows.push_back({own, nullptr});
                folder = std::move(own);
            }
            rows.push_back({{}, change});
        }
        const float form = merging || !repository.userName.empty()
                               ? ImGui::GetTextLineHeightWithSpacing() * MessageLines + ImGui::GetFrameHeight() +
                                     ImGui::GetStyle().ItemSpacing.y * 4
                               : ImGui::GetFrameHeight() * 5;
        const float listHeight = std::max(ImGui::GetFrameHeight() * 3, ImGui::GetContentRegionAvail().y - form);
        // Rows closer than controls: a list of files reads as one thing.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {ImGui::GetStyle().FramePadding.x, RowPadding * ToolUiScale()});
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, {0, 0.5f});
        const float rowHeight = ImGui::GetFrameHeight();
        if (ImGui::BeginChild("git-files", {0, listHeight})) {
            const auto selectedDiff = git.diff ? git.diff->path : std::string{};
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(rows.size()), rowHeight);
            while (clipper.Step())
                for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                    const auto& row = rows[static_cast<std::size_t>(rowIndex)];
                    ImGui::PushID(rowIndex);
                    if (!row.change) {
                        if (!merging) {
                            std::size_t total = 0;
                            std::size_t ticked = 0;
                            for (const auto* change : changes)
                                if (Folder(change->path) == row.folder) {
                                    ++total;
                                    ticked += _selected.contains(change->path) ? 1 : 0;
                                }
                            if (Tick("##folder", ticked == total, ticked > 0)) {
                                for (const auto* change : changes)
                                    if (Folder(change->path) == row.folder) {
                                        if (ticked == total)
                                            _selected.erase(change->path);
                                        else
                                            _selected.insert(change->path);
                                    }
                            }
                            ImGui::SameLine();
                        }
                        ImGui::AlignTextToFramePadding();
                        const auto text = row.folder.empty() ? std::string("Project root") : row.folder;
                        ImGui::TextColored(ToolPalette::Muted, "%s",
                                           ToolClipMiddle(text.c_str(), ImGui::GetContentRegionAvail().x).c_str());
                    } else {
                        const auto& change = *row.change;
                        ImGui::Indent(ImGui::GetFontSize() * 0.6f);
                        if (!merging) {
                            if (Tick("##file", _selected.contains(change.path), false)) {
                                if (_selected.contains(change.path))
                                    _selected.erase(change.path);
                                else
                                    _selected.insert(change.path);
                            }
                            ImGui::SameLine();
                        }
                        const float letter = ImGui::GetFontSize() * 1.3f;
                        const auto start = ImGui::GetCursorPos();
                        const float nameWidth = ImGui::GetContentRegionAvail().x - letter;
                        const auto name = ToolClipMiddle(FileName(change.path).c_str(), nameWidth);
                        if (ImGui::Selectable((name + "###row").c_str(), selectedDiff == change.path,
                                              ImGuiSelectableFlags_AllowOverlap, {0, rowHeight}))
                            Send(actions, GitRequest{.kind = GitRequestKind::Diff, .paths = {change.path}});
                        ToolTooltip(change.originalPath.empty()
                                        ? change.path.c_str()
                                        : (change.originalPath + "  \xE2\x86\x92  " + change.path).c_str());
                        if (ImGui::BeginPopupContextItem("git-file")) {
                            if (ImGui::MenuItem("Open file") && actions.openPath)
                                actions.openPath(
                                    repository.directory /
                                    std::filesystem::path(std::u8string(
                                        reinterpret_cast<const char8_t*>(change.path.data()), change.path.size())));
                            if (change.kind == GitChangeKind::Untracked && ImGui::MenuItem("Add to .gitignore"))
                                Send(actions, GitRequest{.kind = GitRequestKind::Ignore, .paths = {change.path}});
                            ImGui::EndPopup();
                        }
                        ImGui::SameLine(start.x + nameWidth);
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextColored(KindColor(change), "%s", KindLetter(change));
                        ImGui::Unindent(ImGui::GetFontSize() * 0.6f);
                    }
                    ImGui::PopID();
                }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);
    }
    if (!changes.empty() || merging) {
        ImGui::Spacing();
        CommitForm(git, repository, actions);
    }
}

void GitDrawer::Incoming(const GitSnapshot& git, const GitRepositoryState& repository, const EditorUiActions& actions,
                         bool building) {
    const auto& branch = repository.branch;
    if (branch.behind == 0) {
        ToolEmptyState("Nothing to pull", "You have the latest commits.");
        return;
    }
    const bool can = git.busy.empty() && !building && repository.sequence == GitSequence::None;
    const bool diverged = branch.ahead > 0;
    const bool overlapping = !repository.incoming.overlapping.empty();
    if (!repository.incoming.submodules.empty())
        Note("Moves a submodule, which usually means a long rebuild.", EditorInfoColor());
    if (diverged) {
        Note(std::format("You have {} commit{} the remote does not, and it has {} you do not. Merging brings them "
                         "together and may ask you to resolve conflicts.",
                         branch.ahead, branch.ahead == 1 ? "" : "s", branch.behind)
                 .c_str(),
             ToolPalette::Accent);
    } else if (overlapping) {
        Note(std::format("{} of your changed file{} also changed there. Git would refuse to overwrite them.",
                         repository.incoming.overlapping.size(), repository.incoming.overlapping.size() == 1 ? "" : "s")
                 .c_str(),
             ToolPalette::Accent);
    } else {
        Note("Fast-forward. Your changes stay as they are.", ToolPalette::Secondary);
    }
    ImGui::BeginDisabled(!can);
    const float width = ImGui::GetContentRegionAvail().x;
    if (diverged) {
        if (MainButton("Merge###git-do", {width, 0})) Send(actions, GitRequestKind::Merge);
    } else if (overlapping) {
        if (MainButton("Stash & Pull###git-do", {width, 0})) Send(actions, GitRequestKind::StashAndPull);
        ToolTooltip("Saves your changes in the stash, pulls, and puts them back. A conflict keeps the saved copy.");
    } else if (MainButton(std::format("Pull {}###git-do", branch.behind).c_str(), {width, 0})) {
        Send(actions, GitRequestKind::Pull);
    }
    ImGui::EndDisabled();
    if (building) ToolTooltip("Wait for the build to finish.");
    ImGui::Spacing();
    if (overlapping) {
        for (const auto& file : repository.incoming.overlapping)
            ImGui::TextColored(ToolPalette::Secondary, "%s",
                               ToolClipMiddle(file.c_str(), ImGui::GetContentRegionAvail().x).c_str());
        ImGui::Spacing();
    }
    ImGui::Separator();
    if (ImGui::BeginChild("git-commits")) {
        for (const auto& commit : repository.incoming.commits) {
            ImGui::PushID(commit.hash.c_str());
            ImGui::PushTextWrapPos(0);
            ImGui::TextUnformatted(commit.subject.c_str());
            ImGui::PopTextWrapPos();
            ImGui::TextColored(ToolPalette::Muted, "%s  \xC2\xB7  %s  \xC2\xB7  %s", commit.hash.c_str(),
                               commit.author.c_str(), TimeAgo(commit.time).c_str());
            ImGui::Spacing();
            ImGui::PopID();
        }
        if (branch.behind > static_cast<int>(repository.incoming.commits.size()))
            ImGui::TextColored(ToolPalette::Muted, "... and %d more",
                               branch.behind - static_cast<int>(repository.incoming.commits.size()));
    }
    ImGui::EndChild();
}

void GitDrawer::Draw(const GitSnapshot& git, const EditorUiActions& actions, ImVec2 position, ImVec2 size,
                     bool building) {
    if (!_open) return;
    if (git.committed != _committed) {
        _committed = git.committed;
        _message[0] = '\0';
        _selected.clear();
        _lastCommit.reset();
    }
    const float scale = ToolUiScale();
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {Padding * scale, Padding * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Panel);
    ImGui::PushStyleColor(ImGuiCol_Border, ToolPalette::Border);
    if (ImGui::Begin("Source control###editor.git-drawer", nullptr, DrawerFlags)) {
        const auto* repository = git.Active();
        const auto changes = repository ? repository->changes.size() : 0;
        const auto behind = repository ? repository->branch.behind : 0;
        if (ToolSegment((std::format("Changes {}", changes) + "###git-tab-changes").c_str(),
                        _tab == GitDrawerTab::Changes))
            _tab = GitDrawerTab::Changes;
        ImGui::SameLine();
        if (ToolSegment((std::format("Incoming {}", behind) + "###git-tab-incoming").c_str(),
                        _tab == GitDrawerTab::Incoming))
            _tab = GitDrawerTab::Incoming;
        const float closeWidth = ImGui::GetFrameHeight();
        ImGui::SameLine(ImGui::GetContentRegionMax().x - closeWidth);
        if (ToolIconButton("git-close", ICON_FA_XMARK, "Close  (Esc)")) _open = false;
        ImGui::Spacing();
        if (repository && repository->isSubmodule)
            ImGui::TextColored(ToolPalette::Muted, "Submodule %s", repository->name.c_str());
        Problem(git, actions);
        if (git.busy.size() > 0) {
            ToolActivity();
            ImGui::SameLine();
            ImGui::TextColored(EditorInfoColor(), "%s", git.busy.c_str());
            if (git.canCancel) {
                ImGui::SameLine();
                if (ToolButton("Cancel")) Send(actions, GitRequestKind::Cancel);
            }
        }
        if (!repository)
            ToolEmptyState("No repository", "Set up Git from the bar above.");
        else if (_tab == GitDrawerTab::Changes)
            Changes(git, *repository, actions);
        else
            Incoming(git, *repository, actions, building);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}
