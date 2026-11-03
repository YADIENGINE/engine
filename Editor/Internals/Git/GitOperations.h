#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Git/GitTypes.h"

// What git does for the editor, synchronously: the service calls these on its own thread. Each returns the problem to
// show (kind None when fine). Nothing here discards work: pulls only fast-forward unless the developer chose Merge, a
// stash is always named and reported, and there is no force push, reset or discard.

inline constexpr std::uint64_t GitLargeFileBytes = 100ull * 1024 * 1024; // What common hosts refuse without LFS
inline constexpr std::size_t GitMaximumDiffLines = 20000;
inline constexpr std::size_t GitMaximumIncoming = 50;

struct GitActionContext {
    const std::atomic<bool>* cancel = nullptr;
    std::filesystem::path scratch; // Temporary files; created on demand
};

// Facts read once per HEAD position, so a refresh runs one process when nothing moved.
struct GitRepositoryCache {
    std::filesystem::path gitDirectory;
    GitBranchStatus branch;
    std::vector<std::string> incomingFiles;
    std::vector<GitCommitSummary> incomingCommits;
    std::vector<std::string> incomingSubmodules;
    std::vector<std::string> remotes;
    std::vector<std::string> localBranches;
    std::vector<std::string> remoteBranches;
    std::string userName;
    std::string userEmail;
    bool valid = false;
};

// The repository's root for a folder, or the problem that says why there is none (None: simply not a repository).
struct GitRootResult {
    std::optional<std::filesystem::path> root;
    GitProblem problem;
};
[[nodiscard]] GitRootResult FindGitRoot(const GitClient& client);
// Null when status could not be read.
[[nodiscard]] std::optional<GitRepositoryState> ReadGitRepository(const GitClient& client, GitRepositoryCache& cache,
                                                                  bool force);
[[nodiscard]] std::vector<std::string> GitSubmodulePaths(const GitClient& client);
// The initialized ones: only these have a repository to read.
[[nodiscard]] bool GitSubmoduleInitialized(const GitClient& client, const std::string& path);

[[nodiscard]] GitProblem GitFetch(const GitClient& client, GitAccess access, const std::atomic<bool>* cancel);
[[nodiscard]] GitProblem GitPull(const GitClient& client, const GitActionContext& context);
// Saves local changes in a named stash around the pull. A conflict on restoring leaves the stash in place.
[[nodiscard]] GitProblem GitStashAndPull(const GitClient& client, const GitActionContext& context,
                                         bool includeUntracked);
[[nodiscard]] GitProblem GitMergeIncoming(const GitClient& client, const GitActionContext& context);
[[nodiscard]] GitProblem GitAbortSequence(const GitClient& client, GitSequence sequence);
// Moves submodules to the commits this repository records, without ever leaving local work behind.
[[nodiscard]] GitProblem GitSyncSubmodules(const GitClient& client, const GitActionContext& context);
// Commits exactly `paths`, whatever else is staged. During a merge the whole merge is committed instead.
[[nodiscard]] GitProblem GitCommit(const GitClient& client, const GitActionContext& context,
                                   const std::vector<std::string>& paths, const std::string& message, bool allowLarge,
                                   GitSequence sequence);
// Refuses while a submodule's recorded commit exists only on this machine: nobody else could check it out.
[[nodiscard]] GitProblem GitPush(const GitClient& client, const GitActionContext& context,
                                 const std::vector<std::string>& remotes);
[[nodiscard]] GitProblem GitPushSubmodules(const GitClient& client, const GitActionContext& context);
// `templateDirectory` holds the engine's project template; its ignore rules keep builds out of the repository.
[[nodiscard]] GitProblem GitInitialize(const GitClient& client, const std::filesystem::path& templateDirectory);
[[nodiscard]] GitProblem GitAddRemote(const GitClient& client, const std::string& url);
[[nodiscard]] GitProblem GitSwitchBranch(const GitClient& client, const std::string& branch);
[[nodiscard]] GitProblem GitCreateBranch(const GitClient& client, const std::string& branch);
[[nodiscard]] GitProblem GitSetIdentity(const GitClient& client, const std::string& name, const std::string& email);
[[nodiscard]] GitProblem GitTakeConflictSide(const GitClient& client, const GitChange& change, GitConflictSide side,
                                             GitSequence sequence);
[[nodiscard]] GitProblem GitMarkResolved(const GitClient& client, const std::string& path);
[[nodiscard]] GitProblem GitIgnorePath(const GitClient& client, const std::string& path);
[[nodiscard]] GitProblem GitRemoveLock(const GitClient& client);
// `problemDetail` is git's refusal: it names the exact folder to trust.
[[nodiscard]] GitProblem GitTrustDirectory(const GitClient& client, const std::string& problemDetail);
[[nodiscard]] GitDiff ReadGitDiff(const GitClient& client, const GitChange& change, const GitActionContext& context);
// `folder` as the repository names it, ending in '/'; empty when it is outside the repository.
[[nodiscard]] std::string GitPathInside(const std::filesystem::path& folder, const std::filesystem::path& repository);
