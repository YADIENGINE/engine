#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// What the editor's source control shows (Docs/Editor/Usage.md → Source control). Plain data: the service publishes one
// immutable GitSnapshot at a time, so the UI thread never waits for git.

enum class GitAvailability { Checking, Missing, NoRepository, Ready };
enum class GitChangeKind { Modified, Added, Deleted, Renamed, Copied, TypeChanged, Untracked, Conflicted };
// A merge or rebase someone left open: it decides what Commit means and what may run.
enum class GitSequence { None, Merge, Rebase, CherryPick, Revert, Bisect };
// The one action the bar brings forward.
enum class GitStep { None, Resolve, Pull, Commit, Push };
// clang-format off
enum class GitProblemKind { None, Offline, Authentication, LocalChanges, Diverged, PushRejected, UnpushedSubmodule, SubmoduleNotUpdated, LfsMissing, Locked, DubiousOwnership, Identity, LargeFiles, Conflicts, Canceled, BuildRunning, Failed };
enum class GitRequestKind { Refresh, Fetch, Pull, StashAndPull, Merge, AbortSequence, Commit, Push, PushSubmodules, Initialize, AddRemote, SwitchBranch, CreateBranch, SelectRepository, SetIdentity, Diff, CloseDiff, TakeConflictSide, MarkResolved, Ignore, RemoveLock, TrustDirectory, UpdateSubmodules, Dismiss, Cancel };
// clang-format on
enum class GitConflictSide { Mine, Theirs };
enum class GitDiffKind { None, Loading, Text, Image, Audio, Binary, Folder };
enum class GitDiffLineKind { Context, Added, Removed, Hunk };

struct GitChange {
    std::string path;         // Repository-relative with '/'; an untracked folder ends in '/'
    std::string originalPath; // Renames and copies
    char index = '.';         // Porcelain v2's X and Y; '.' is unchanged
    char worktree = '.';
    GitChangeKind kind = GitChangeKind::Modified;
    bool submodule = false; // A submodule whose recorded commit changed
    bool operator==(const GitChange&) const = default;
};

struct GitBranchStatus {
    std::string head; // Branch name; empty when detached
    std::string oid;  // Empty on an unborn branch
    std::string upstream;
    int ahead = 0;
    int behind = 0;
    bool upstreamGone = false; // Configured, but the remote branch no longer exists
    bool operator==(const GitBranchStatus&) const = default;
};

struct GitStatus {
    GitBranchStatus branch;
    std::vector<GitChange> changes;
};

struct GitCommitSummary {
    std::string hash;
    std::string subject;
    std::string author;
    std::int64_t time = 0; // Seconds since the epoch
};

struct GitDiffPath {
    std::string path;
    bool submodule = false;
};

// What a Pull would bring.
struct GitIncoming {
    std::vector<GitCommitSummary> commits;
    std::vector<std::string> submodules;  // Whose recorded commit moves: usually a long rebuild
    std::vector<std::string> overlapping; // Files changed both here and there
};

struct GitRepositoryState {
    std::string name;
    std::filesystem::path directory;
    bool isSubmodule = false;
    GitBranchStatus branch;
    std::vector<GitChange> changes;
    GitSequence sequence = GitSequence::None;
    std::vector<std::string> remotes;
    std::vector<std::string> localBranches;
    std::vector<std::string> remoteBranches;
    std::string userName;
    std::string userEmail;
    GitIncoming incoming;
    std::optional<std::chrono::system_clock::time_point> fetchedAt;
};

struct GitProblem {
    GitProblemKind kind = GitProblemKind::None;
    std::string title;
    std::string detail;
    std::vector<std::string> files;
};

struct GitDiffLine {
    GitDiffLineKind kind = GitDiffLineKind::Context;
    std::string text;
};

struct GitDiff {
    GitDiffKind kind = GitDiffKind::None;
    std::string path;
    std::vector<GitDiffLine> lines;
    bool truncated = false;
    std::filesystem::path before; // Image and Audio; empty when the file did not exist on that side
    std::filesystem::path after;
};

struct GitSnapshot {
    GitAvailability availability = GitAvailability::Checking;
    std::vector<GitRepositoryState> repositories; // The project's repository first, then its submodules
    std::size_t active = 0;
    std::string busy; // What runs for the developer, e.g. "Pulling"; empty when idle
    bool canCancel = false;
    bool fetching = false;   // Any fetch, automatic or asked for: the bar's buttons animate
    GitProblem problem;      // The last action's failure, until dismissed or the next action
    GitProblem fetchProblem; // Why the background fetch could not run
    std::string notice;      // The last action's outcome
    std::shared_ptr<const GitDiff> diff;
    std::uint64_t committed = 0;      // Counts commits made here: the drawer clears its message and ticks
    std::uint64_t sourcesChanged = 0; // Counts actions that moved HEAD or the working tree: the host builds

    [[nodiscard]] const GitRepositoryState* Active() const {
        return active < repositories.size() ? &repositories[active] : nullptr;
    }
};

struct GitRequest {
    GitRequestKind kind = GitRequestKind::Refresh;
    std::vector<std::string> paths;
    std::string text;       // Commit message, remote URL, branch name or user name
    std::string secondText; // User email
    std::size_t index = 0;  // SelectRepository
    GitConflictSide side = GitConflictSide::Mine;
    bool thenPush = false;   // Commit
    bool allowLarge = false; // Commit: the developer saw the large file warning
};
