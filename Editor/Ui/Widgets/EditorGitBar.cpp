#include "Editor/Ui/Widgets/EditorGitBar.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <format>
#include "Editor/Internals/Git/GitParsing.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float Gap = 6.0f;
constexpr float GroupGap = 12.0f;
constexpr float ButtonMinimumWidth = 62.0f;
constexpr float SteadyTint = 0.22f;
constexpr float QuietTint = 0.16f;
constexpr double NoticeSeconds = 6;
constexpr const char* GetGitUrl = "https://git-scm.com/downloads";
constexpr int MaximumTooltipCommits = 8;

enum class BarLook { Quiet, Amber, Pulse, Rose };
void Send(const EditorUiActions& actions, GitRequestKind kind) {
    if (actions.git) actions.git(GitRequest{.kind = kind});
}
std::string AgeLabel(std::chrono::system_clock::time_point time) {
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - time).count();
    if (seconds < 60) return "Fetched just now";
    if (seconds < 3600) return std::format("Fetched {} min ago", seconds / 60);
    if (seconds < 86400) return std::format("Fetched {} h ago", seconds / 3600);
    return std::format("Fetched {} d ago", seconds / 86400);
}
float ButtonWidth(const std::string& label) {
    return std::max(ImGui::CalcTextSize(label.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2,
                    ButtonMinimumWidth * ToolUiScale());
}
// `loadingIcon`: the label's leading icon, which the activity indicator replaces in place so nothing shifts.
bool BarButton(const std::string& label, bool enabled, BarLook look, float width, const std::string& tooltip,
               const char* loadingIcon = nullptr) {
    const auto tone = look == BarLook::Rose ? ToolToneColor(ToolTone::Error) : ToolColors().accent;
    const float amount = look == BarLook::Pulse ? EditorAttentionPulse() : look == BarLook::Quiet ? 0.0f : SteadyTint;
    ImGui::BeginDisabled(!enabled);
    const bool tinted = enabled && look != BarLook::Quiet;
    if (tinted) {
        ImGui::PushStyleColor(ImGuiCol_Button, EditorTintSurface(ToolColors().control, tone, amount));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorTintSurface(ToolColors().control, tone, amount + 0.1f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorTintSurface(ToolColors().control, tone, amount * 0.6f));
        ImGui::PushStyleColor(ImGuiCol_Text, look == BarLook::Rose ? EditorStopTextColor() : ToolColors().accent);
    }
    const auto idMark = label.find("###");
    const bool pressed =
        ToolButton(loadingIcon ? label.c_str() + idMark : label.c_str(), {width, ImGui::GetFrameHeight()});
    if (loadingIcon) {
        // Measured as ToolButton draws it, so the words stay where they were.
        const auto visible = label.substr(0, idMark);
        const auto from = ImGui::GetItemRectMin();
        const auto to = ImGui::GetItemRectMax();
        ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
        const auto size = ImGui::CalcTextSize(visible.c_str());
        const float iconWidth = ImGui::CalcTextSize(loadingIcon).x;
        const float x = from.x + (to.x - from.x - size.x) * 0.5f;
        const float side = ToolActivitySide();
        auto color = enabled ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ToolColors().disabled;
        if (!enabled)
            color.w /= std::max(ImGui::GetStyle().Alpha, 0.001f); // As ToolButton's disabled text: no second fade
        auto* draw = ImGui::GetWindowDrawList();
        // The engine's activity amber, full strength on a disabled button too: the work is running either way.
        auto activity = ToolColors().accent;
        activity.w /= std::max(ImGui::GetStyle().Alpha, 0.001f);
        ToolDrawActivity(*draw, {x + (iconWidth - side) * 0.5f, (from.y + to.y - side) * 0.5f}, activity, true);
        draw->AddText({x + iconWidth, (from.y + to.y - size.y) * 0.5f}, ImGui::GetColorU32(color),
                      visible.c_str() + std::string_view(loadingIcon).size());
        ImGui::PopFont();
    }
    if (tinted) ImGui::PopStyleColor(4);
    ImGui::EndDisabled();
    if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_ForTooltip))
        ImGui::SetTooltip("%s", tooltip.c_str());
    return pressed && enabled;
}
void CenteredCell(ImVec4 color) {
    const float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(y + ImGui::GetStyle().FramePadding.y);
    ToolStateCell(color);
    ImGui::SameLine(0, Gap * ToolUiScale());
    ImGui::SetCursorPosY(y);
}
void BarText(const std::string& text, ImVec4 color) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(color, "%s", text.c_str());
}
std::string IncomingTooltip(const GitRepositoryState& repository) {
    std::string text;
    int shown = 0;
    for (const auto& commit : repository.incoming.commits) {
        if (shown++ == MaximumTooltipCommits) {
            text += std::format("... and {} more\n", repository.branch.behind - MaximumTooltipCommits);
            break;
        }
        text += commit.subject + "  \xC2\xB7  " + commit.author + "\n";
    }
    if (!repository.incoming.submodules.empty()) {
        text += "\nMoves a submodule (";
        for (const auto& path : repository.incoming.submodules)
            text += path + " ";
        text.back() = ')';
        text += ": usually a long rebuild.";
    }
    if (repository.branch.ahead > 0)
        text += "\nThis branch has its own commits too. Click to review the merge.";
    else if (!repository.incoming.overlapping.empty())
        text += std::format("\n{} of your changed files also changed there. Click to review.",
                            repository.incoming.overlapping.size());
    else
        text += "\nFast-forward: your changes stay as they are.";
    return text;
}
} // namespace

void EditorGitBar::Repositories(const GitSnapshot& git, const GitRepositoryState& active,
                                const EditorUiActions& actions, bool building) {
    const bool locked = !git.busy.empty() || building;
    if (git.repositories.size() > 1) {
        ImGui::SeparatorText("Repository");
        for (std::size_t index = 0; index < git.repositories.size(); ++index) {
            const auto& repository = git.repositories[index];
            auto label = repository.name;
            if (repository.branch.behind > 0)
                label += std::format("  " ICON_FA_ARROW_DOWN " {}", repository.branch.behind);
            if (repository.branch.ahead > 0) label += std::format("  " ICON_FA_ARROW_UP " {}", repository.branch.ahead);
            if (!repository.changes.empty()) label += std::format("  \xC2\xB7  {} changed", repository.changes.size());
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::MenuItem(label.c_str(), nullptr, index == git.active) && actions.git)
                actions.git(GitRequest{.kind = GitRequestKind::SelectRepository, .index = index});
            ImGui::PopID();
        }
    }
    ImGui::SeparatorText("Branch");
    ImGui::BeginDisabled(locked);
    for (const auto& branch : active.localBranches)
        if (ImGui::MenuItem(branch.c_str(), nullptr, branch == active.branch.head) && branch != active.branch.head &&
            actions.git)
            actions.git(GitRequest{.kind = GitRequestKind::SwitchBranch, .text = branch});
    // Teammates' branches, by the short name `git switch` turns into a local one.
    bool heading = false;
    for (const auto& remoteBranch : active.remoteBranches) {
        const auto slash = remoteBranch.find('/');
        if (slash == std::string::npos) continue;
        const auto name = remoteBranch.substr(slash + 1);
        if (std::ranges::find(active.localBranches, name) != active.localBranches.end()) continue;
        if (!std::exchange(heading, true)) ImGui::SeparatorText("On the remote");
        if (ImGui::MenuItem(remoteBranch.c_str()) && actions.git)
            actions.git(GitRequest{.kind = GitRequestKind::SwitchBranch, .text = name});
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14);
    const bool create = ImGui::InputTextWithHint("##new-branch", "New branch name", _newBranch.data(),
                                                 _newBranch.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::BeginDisabled(locked || _newBranch[0] == '\0');
    if ((ToolButton("Create") || create) && _newBranch[0] != '\0' && actions.git) {
        actions.git(GitRequest{.kind = GitRequestKind::CreateBranch, .text = _newBranch.data()});
        _newBranch[0] = '\0';
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
}

EditorGitBarResult EditorGitBar::Draw(const GitSnapshot& git, const EditorUiActions& actions,
                                      const std::string& projectName, bool building) {
    EditorGitBarResult result;
    const float scale = ToolUiScale();
    const float gap = Gap * scale;
    const float groupGap = GroupGap * scale;
    const float cell = ImGui::GetFontSize() * 0.6f + gap;
    const float rowEnd = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2;
    const float available = rowEnd - ImGui::GetCursorPosX() - groupGap;
    const auto rightAlign = [&](float width) { ImGui::SameLine(rowEnd - width); };

    if (git.availability != GitAvailability::Ready) {
        const bool untrusted = git.fetchProblem.kind == GitProblemKind::DubiousOwnership;
        const std::string message = git.availability == GitAvailability::Checking  ? std::string{}
                                    : git.availability == GitAvailability::Missing ? "Git is not installed"
                                    : untrusted                                    ? "Git does not trust this folder"
                                                                                   : "Not in version control";
        const std::string button = git.availability == GitAvailability::Checking  ? std::string{}
                                   : git.availability == GitAvailability::Missing ? "Get Git"
                                   : untrusted                                    ? "Trust folder"
                                                                                  : "Set up Git";
        float width = ImGui::CalcTextSize(projectName.c_str()).x;
        if (!message.empty()) width += groupGap + ImGui::CalcTextSize(message.c_str()).x + gap + ButtonWidth(button);
        if (width > available) return result;
        rightAlign(width);
        BarText(projectName, ToolColors().muted);
        if (!message.empty()) {
            ImGui::SameLine(0, groupGap);
            BarText(message, ToolColors().secondary);
            ImGui::SameLine(0, gap);
            const std::string tooltip = git.availability == GitAvailability::Missing
                                            ? "Opens the download page. The editor notices when you come back."
                                        : untrusted
                                            ? "This folder belongs to another Windows user. Trusting it adds it "
                                              "to safe.directory in your git settings."
                                            : "Starts a git repository here and adds ignore rules, so build "
                                              "folders stay out. Nothing leaves this computer.";
            if (BarButton(button, true, BarLook::Amber, ButtonWidth(button), tooltip)) {
                if (git.availability == GitAvailability::Missing && actions.openPath)
                    actions.openPath(GetGitUrl);
                else
                    Send(actions, untrusted ? GitRequestKind::TrustDirectory : GitRequestKind::Initialize);
            }
        }
        return result;
    }
    const auto* repository = git.Active();
    if (!repository) return result;

    // A failure the developer's click caused opens its explanation.
    const auto problemKey =
        git.problem.kind == GitProblemKind::None ? std::string{} : git.problem.title + git.problem.detail;
    if (problemKey != _lastProblem) {
        _lastProblem = problemKey;
        if (!problemKey.empty()) result.show = GitDrawerTab::Changes;
    }
    const double now = ImGui::GetTime();
    if (git.notice != _lastNotice) {
        _lastNotice = git.notice;
        _noticeTime = now;
    }
    _attention.Update(repository->branch.behind, now);

    const auto& branch = repository->branch;
    const auto step = NextGitStep(*repository);
    const auto conflicts =
        static_cast<int>(std::ranges::count(repository->changes, GitChangeKind::Conflicted, &GitChange::kind));
    const bool idle = git.busy.empty();
    const bool detached = branch.head.empty();
    const bool otherSequence = repository->sequence != GitSequence::None && repository->sequence != GitSequence::Merge;
    const auto changed = static_cast<int>(repository->changes.size());

    // Pull
    const bool canPull = !branch.upstream.empty() && !branch.upstreamGone && branch.behind > 0 && conflicts == 0 &&
                         repository->sequence == GitSequence::None && !detached;
    const bool diverged = canPull && branch.ahead > 0;
    const std::string pullText = diverged ? "Merge" : "Pull";
    const auto pullLook = !canPull || step != GitStep::Pull ? BarLook::Quiet
                          : _attention.Pulsing(now)         ? BarLook::Pulse
                                                            : BarLook::Amber;
    std::string pullTooltip = building                  ? "Wait for the build to finish."
                              : canPull                 ? IncomingTooltip(*repository)
                              : detached                ? "Not on a branch."
                              : branch.upstream.empty() ? "This branch is not on a remote yet."
                              : branch.upstreamGone     ? "The remote branch no longer exists."
                                                        : "You have the latest commits.";
    if (repository->fetchedAt) pullTooltip += "\n\n" + AgeLabel(*repository->fetchedAt);
    // Commit, or Resolve while files conflict
    const bool resolving = conflicts > 0;
    const bool canCommit = changed > 0 || repository->sequence == GitSequence::Merge;
    const auto commitLook = resolving ? BarLook::Rose : step == GitStep::Commit ? BarLook::Amber : BarLook::Quiet;
    const std::string commitTooltip = resolving   ? "Some files need a decision before you can continue."
                                      : canCommit ? "Choose files and write a message  (Ctrl+Shift+G)"
                                                  : "Nothing has changed since the last commit.";
    // Push, or Publish while the remote does not know this branch
    const bool hasCommit = !branch.oid.empty();
    const bool unpublished = !detached && hasCommit && (branch.upstream.empty() || branch.upstreamGone);
    const bool canPush = !detached && hasCommit && (unpublished || (branch.ahead > 0 && branch.behind == 0));
    const std::string pushText = unpublished ? "Publish" : "Push";
    const auto pushLook = canPush && step == GitStep::Push ? BarLook::Amber : BarLook::Quiet;
    const std::string pushTooltip = !hasCommit ? "Make the first commit before publishing."
                                    : detached ? "Not on a branch."
                                    : unpublished
                                        ? (repository->remotes.empty() ? "Put this project on a remote such as GitHub."
                                                                       : "Send this branch to the remote.")
                                    : branch.behind > 0 ? "Pull first: the remote has commits you do not have."
                                    : branch.ahead > 0  ? "Send your commits to the remote."
                                                        : "Everything is on the remote.";

    const auto label = [](const char* icon, const std::string& text, int count, bool showText, const char* id) {
        std::string result = icon;
        if (showText) result += " " + text;
        if (count > 0) result += " " + std::to_string(count);
        return result + "###" + id;
    };
    // The detail on the left goes first as the window narrows.
    const std::string name = repository->name;
    const std::string branchName = detached ? "detached " + branch.oid.substr(0, 7) : branch.head;
    std::string status;
    auto statusColor = ToolColors().muted;
    bool showAction = false;                  // Sign in again
    if (!git.busy.empty() && !git.fetching) { // A fetch shows on the buttons instead
        status = git.busy;
        statusColor = EditorInfoColor();
    } else if (git.problem.kind != GitProblemKind::None) {
        status = "Needs attention";
        statusColor = ToolToneColor(ToolTone::Error);
    } else if (!git.notice.empty() && now - _noticeTime < NoticeSeconds) {
        status = git.notice;
        statusColor = ToolToneColor(ToolTone::Success);
    } else if (git.fetchProblem.kind == GitProblemKind::Authentication) {
        status = "Sign-in needed";
        statusColor = ToolColors().accent;
        showAction = true;
    } else if (git.fetchProblem.kind != GitProblemKind::None) {
        status = "Offline";
    }
    const auto widthOf = [&](bool withName, bool withStatus, bool withText) {
        float width = cell + ImGui::CalcTextSize(branchName.c_str()).x + ImGui::CalcTextSize(ICON_FA_CHEVRON_DOWN).x +
                      ImGui::GetFontSize() * 2.2f + groupGap;
        if (withName) width += ImGui::CalcTextSize(name.c_str()).x + gap;
        if (withStatus && !status.empty()) width += ImGui::CalcTextSize(status.c_str()).x + groupGap;
        width += ButtonWidth(label(ICON_FA_ROTATE, "Fetch", 0, withText, "f"));
        width += ButtonWidth(label(ICON_FA_ARROW_DOWN, pullText, branch.behind, withText, "p"));
        width += ButtonWidth(
            label(ICON_FA_CHECK, resolving ? "Resolve" : "Commit", resolving ? conflicts : changed, withText, "c"));
        width += ButtonWidth(label(ICON_FA_ARROW_UP, pushText, branch.ahead, withText, "s"));
        return width + gap * 4;
    };
    bool showName = true;
    bool showStatus = true;
    bool showText = true;
    if (widthOf(showName, showStatus, showText) > available) showStatus = false;
    if (widthOf(showName, showStatus, showText) > available) showName = false;
    if (widthOf(showName, showStatus, showText) > available) showText = false;
    const float total = widthOf(showName, showStatus, showText);
    if (total > available) return result; // Narrower than the menus leave room for; the palette and shortcut remain
    rightAlign(total);

    const auto stateColor = resolving                                       ? ToolToneColor(ToolTone::Error)
                            : git.fetchProblem.kind != GitProblemKind::None ? ToolColors().muted
                            : step == GitStep::None                         ? ToolToneColor(ToolTone::Success)
                                                                            : ToolColors().accent;
    CenteredCell(stateColor);
    if (showName) {
        BarText(name, ToolColors().secondary);
        ImGui::SameLine(0, gap);
    }
    // The branch: its menu holds the repository's submodules and the other branches.
    const auto branchLabel =
        std::string(ICON_FA_CODE_BRANCH " ") + branchName + " " ICON_FA_CHEVRON_DOWN "###git-branch";
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, detached ? ToolColors().accent : ToolColors().text);
    if (ToolButton(branchLabel.c_str())) {
        ImGui::OpenPopup("git-repositories");
        Send(actions, GitRequestKind::Refresh);
    }
    ImGui::PopStyleColor(2);
    if (detached) ToolTooltip("Not on a branch: commits made here are easy to lose. Choose a branch.");
    if (ImGui::BeginPopup("git-repositories")) {
        Repositories(git, *repository, actions, building);
        ImGui::EndPopup();
    }
    if (showStatus && !status.empty()) {
        ImGui::SameLine(0, groupGap);
        if (showAction) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0, 0, 0, 0});
            ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
            if (ToolButton((status + "###git-sign-in").c_str())) Send(actions, GitRequestKind::Fetch);
            ImGui::PopStyleColor(2);
            ToolTooltip("The remote refused the last fetch. Click to sign in. Fetching stays paused until then.");
        } else {
            BarText(status, statusColor);
            if (git.canCancel) {
                ToolTooltip("Click to cancel.");
                if (ImGui::IsItemClicked()) Send(actions, GitRequestKind::Cancel);
            } else if (git.problem.kind != GitProblemKind::None) {
                ToolTooltip(git.problem.title.c_str());
                if (ImGui::IsItemClicked()) result.show = GitDrawerTab::Changes;
            } else if (git.fetchProblem.kind != GitProblemKind::None) {
                ToolTooltip(git.fetchProblem.detail.c_str());
            }
        }
    }
    ImGui::SameLine(0, groupGap);
    const bool hasRemote = !repository->remotes.empty();
    std::string fetchTooltip = hasRemote ? "Check the remote for new commits." : "This repository has no remote yet.";
    if (hasRemote && repository->fetchedAt) fetchTooltip += "\n\n" + AgeLabel(*repository->fetchedAt);
    const auto fetch = label(ICON_FA_ROTATE, "Fetch", 0, showText, "git-fetch");
    if (BarButton(fetch, hasRemote && idle && !git.fetching, BarLook::Quiet, ButtonWidth(fetch), fetchTooltip,
                  git.fetching ? ICON_FA_ROTATE : nullptr))
        Send(actions, GitRequestKind::Fetch);
    ImGui::SameLine(0, gap);
    const auto pull = label(ICON_FA_ARROW_DOWN, pullText, branch.behind, showText, "git-pull");
    if (BarButton(pull, canPull && idle && !building, pullLook, ButtonWidth(pull), pullTooltip,
                  git.fetching ? ICON_FA_ARROW_DOWN : nullptr)) {
        if (diverged || !repository->incoming.overlapping.empty())
            result.show = GitDrawerTab::Incoming;
        else
            Send(actions, GitRequestKind::Pull);
    }
    ImGui::SameLine(0, gap);
    const char* commitIcon = resolving ? ICON_FA_TRIANGLE_EXCLAMATION : ICON_FA_CHECK;
    const auto commit =
        label(commitIcon, resolving ? "Resolve" : "Commit", resolving ? conflicts : changed, showText, "git-commit");
    if (BarButton(commit, (canCommit || otherSequence) && idle, commitLook, ButtonWidth(commit), commitTooltip,
                  git.fetching ? commitIcon : nullptr))
        result.toggle = GitDrawerTab::Changes;
    ImGui::SameLine(0, gap);
    const auto push = label(ICON_FA_ARROW_UP, pushText, branch.ahead, showText, "git-push");
    if (BarButton(push, canPush && idle, pushLook, ButtonWidth(push), pushTooltip,
                  git.fetching ? ICON_FA_ARROW_UP : nullptr)) {
        if (repository->remotes.empty())
            result.show = GitDrawerTab::Changes; // Asks for the address
        else
            Send(actions, GitRequestKind::Push);
    }
    return result;
}
