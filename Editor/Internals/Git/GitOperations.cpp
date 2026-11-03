#include "Editor/Internals/Git/GitOperations.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <thread>
#include <utility>
#include "Editor/Internals/Git/GitParsing.h"

namespace {
constexpr std::chrono::milliseconds NoTimeout{0};
constexpr int LockRetries = 4;
constexpr std::chrono::milliseconds LockRetryDelay{500};
constexpr std::string_view StashMessage = "Saved by the editor before a pull";

std::filesystem::path FromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
std::string FirstLine(const std::string& text) {
    const auto lines = SplitGitLines(text);
    return lines.empty() ? std::string{} : lines.front();
}
bool Fine(const GitProblem& problem) { return problem.kind == GitProblemKind::None; }
GitProblem Problem(GitProblemKind kind, std::string title, std::string detail = {},
                   std::vector<std::string> files = {}) {
    return {kind, std::move(title), std::move(detail), std::move(files)};
}
GitProblem ProblemOf(const GitResult& result) {
    if (result.canceled) return Problem(GitProblemKind::Canceled, "Canceled.");
    if (result.timedOut) return Problem(GitProblemKind::Offline, "The remote did not answer in time.");
    if (!result.started) return Problem(GitProblemKind::Failed, "Could not start git.", result.errors);
    // Merge reports conflicts on stdout, everything else on stderr.
    return ClassifyGitError(result.errors + "\n" + result.output);
}
// A command that needs the index waits out another git (often an agent's) instead of failing at once.
GitResult Action(const GitClient& client, const std::vector<std::string>& arguments, const GitActionContext& context,
                 GitAccess access = GitAccess::Interactive) {
    GitResult result;
    for (int attempt = 0; attempt < LockRetries; ++attempt) {
        result = client.Run(arguments, access, context.cancel, NoTimeout);
        if (result.ok || result.errors.find("index.lock") == std::string::npos) break;
        std::this_thread::sleep_for(LockRetryDelay);
    }
    return result;
}
GitProblem Checked(const GitResult& result) { return result.ok ? GitProblem{} : ProblemOf(result); }

// A temporary list of paths: a command line cannot hold thousands of them.
class PathspecFile {
public:
    PathspecFile(const std::filesystem::path& scratch, const std::vector<std::string>& paths) {
        std::error_code error;
        std::filesystem::create_directories(scratch, error);
        _file = scratch / "pathspec.txt";
        std::ofstream out(_file, std::ios::binary);
        for (const auto& path : paths)
            out.write(path.c_str(), static_cast<std::streamsize>(path.size() + 1));
    }
    ~PathspecFile() {
        std::error_code error;
        std::filesystem::remove(_file, error);
    }
    [[nodiscard]] std::string Argument() const {
        const auto text = _file.generic_u8string();
        return "--pathspec-from-file=" + std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

private:
    std::filesystem::path _file;

public:
    PathspecFile(const PathspecFile&) = delete;
    PathspecFile& operator=(const PathspecFile&) = delete;
};

GitSequence ReadSequence(const std::filesystem::path& gitDirectory) {
    std::error_code error;
    const auto has = [&](const char* name) { return std::filesystem::exists(gitDirectory / name, error); };
    if (has("rebase-merge") || has("rebase-apply")) return GitSequence::Rebase;
    if (has("MERGE_HEAD")) return GitSequence::Merge;
    if (has("CHERRY_PICK_HEAD")) return GitSequence::CherryPick;
    if (has("REVERT_HEAD")) return GitSequence::Revert;
    if (has("BISECT_LOG")) return GitSequence::Bisect;
    return GitSequence::None;
}

// FETCH_HEAD's time also counts fetches from a terminal or an agent, and earlier sessions.
std::optional<std::chrono::system_clock::time_point> ReadFetchTime(const std::filesystem::path& gitDirectory) {
    std::error_code error;
    const auto written = std::filesystem::last_write_time(gitDirectory / "FETCH_HEAD", error);
    if (error) return std::nullopt;
    return std::chrono::system_clock::now() - std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                                  std::filesystem::file_time_type::clock::now() - written);
}

void ReadSlowFacts(const GitClient& client, GitRepositoryCache& cache, const GitBranchStatus& branch) {
    if (cache.gitDirectory.empty()) {
        const auto directory = client.Run({"rev-parse", "--absolute-git-dir"});
        if (directory.ok) cache.gitDirectory = FromUtf8(FirstLine(directory.output));
    }
    cache.remotes = SplitGitLines(client.Run({"remote"}).output);
    cache.localBranches.clear();
    cache.remoteBranches.clear();
    constexpr std::string_view Local = "refs/heads/";
    constexpr std::string_view Remote = "refs/remotes/";
    for (const auto& reference :
         SplitGitLines(client.Run({"for-each-ref", "--format=%(refname)", "refs/heads", "refs/remotes"}).output)) {
        if (reference.starts_with(Local)) cache.localBranches.push_back(reference.substr(Local.size()));
        if (reference.starts_with(Remote) && !reference.ends_with("/HEAD"))
            cache.remoteBranches.push_back(reference.substr(Remote.size()));
    }
    cache.userName = FirstLine(client.Run({"config", "user.name"}).output);
    cache.userEmail = FirstLine(client.Run({"config", "user.email"}).output);
    cache.incomingCommits.clear();
    cache.incomingFiles.clear();
    cache.incomingSubmodules.clear();
    if (branch.behind > 0 && !branch.oid.empty()) {
        cache.incomingCommits = ParseGitLog(client
                                                .Run({"log", "-n", std::to_string(GitMaximumIncoming),
                                                      "--format=%h%x1f%s%x1f%an%x1f%ct%x1e", "HEAD..@{upstream}"})
                                                .output);
        // Against where the histories split: what the remote changed, not what this side changed.
        for (const auto& changed : ParseGitRawDiff(
                 client.Run({"diff", "--raw", "-z", "--no-renames", "--no-abbrev", "HEAD...@{upstream}"}).output)) {
            cache.incomingFiles.push_back(changed.path);
            if (changed.submodule) cache.incomingSubmodules.push_back(changed.path);
        }
    }
    cache.branch = branch;
    cache.valid = true;
}

GitProblem FetchedPull(const GitClient& client, const GitActionContext& context,
                       const std::vector<std::string>& merge) {
    if (auto fetched = GitFetch(client, GitAccess::Interactive, context.cancel); !Fine(fetched)) return fetched;
    auto problem = Checked(Action(client, merge, context));
    if (!Fine(problem)) return problem;
    return GitSyncSubmodules(client, context);
}

std::string RecordedCommit(const GitClient& client, const std::string& path) {
    const auto result = client.Run({"rev-parse", "HEAD:" + path});
    return result.ok ? FirstLine(result.output) : std::string{};
}

bool IsAncestor(const GitClient& client, const std::string& ancestor, const std::string& descendant) {
    return client.Run({"merge-base", "--is-ancestor", ancestor, descendant}).ok;
}

std::vector<std::string> LargeUnfilteredFiles(const GitClient& client, const std::vector<std::string>& paths) {
    std::vector<std::string> large;
    std::error_code error;
    const auto check = [&](const std::string& path) {
        const auto file = client.Directory() / FromUtf8(path);
        if (std::filesystem::is_regular_file(file, error) &&
            std::filesystem::file_size(file, error) >= GitLargeFileBytes)
            large.push_back(path);
    };
    for (const auto& path : paths) {
        if (path.ends_with('/')) { // A new folder: its files are what gets committed
            for (const auto& file :
                 SplitGitNul(client.Run({"ls-files", "-o", "--exclude-standard", "-z", "--", path}).output))
                check(file);
        } else {
            check(path);
        }
    }
    if (large.empty()) return large;
    std::vector<std::string> arguments{"check-attr", "filter", "-z", "--"};
    arguments.insert(arguments.end(), large.begin(), large.end());
    const auto filtered = ParseGitLfsPaths(client.Run(arguments).output);
    std::erase_if(large, [&](const std::string& path) { return std::ranges::find(filtered, path) != filtered.end(); });
    return large;
}

std::string ReadWholeFile(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void WriteTemplateFile(const std::filesystem::path& templateDirectory, const std::filesystem::path& target,
                       const char* templateName, std::string_view fallback) {
    std::error_code error;
    if (std::filesystem::exists(target, error)) return; // Never replace the developer's rules
    auto text = ReadWholeFile(templateDirectory / templateName);
    if (text.empty()) text = fallback;
    std::ofstream(target, std::ios::binary) << text;
}
} // namespace

GitRootResult FindGitRoot(const GitClient& client) {
    const auto result = client.Run({"rev-parse", "--show-toplevel"});
    GitRootResult root;
    if (result.ok) {
        root.root = FromUtf8(FirstLine(result.output));
        return root;
    }
    if (result.errors.find("not a git repository") == std::string::npos) root.problem = ProblemOf(result);
    return root;
}

std::optional<GitRepositoryState> ReadGitRepository(const GitClient& client, GitRepositoryCache& cache, bool force) {
    const auto read = client.Run({"status", "--porcelain=v2", "--branch", "-z", "--untracked-files=normal",
                                  "--ignore-submodules=dirty", "--find-renames"});
    if (!read.ok) return std::nullopt;
    auto status = ParseGitStatus(read.output);
    if (force || !cache.valid || !(cache.branch == status.branch)) ReadSlowFacts(client, cache, status.branch);
    GitRepositoryState state;
    state.directory = client.Directory();
    state.branch = std::move(status.branch);
    state.changes = std::move(status.changes);
    state.sequence = ReadSequence(cache.gitDirectory);
    state.remotes = cache.remotes;
    state.localBranches = cache.localBranches;
    state.remoteBranches = cache.remoteBranches;
    state.userName = cache.userName;
    state.userEmail = cache.userEmail;
    state.fetchedAt = ReadFetchTime(cache.gitDirectory);
    state.incoming.commits = cache.incomingCommits;
    state.incoming.submodules = cache.incomingSubmodules;
    state.incoming.overlapping = GitOverlap(state.changes, cache.incomingFiles);
    return state;
}

std::vector<std::string> GitSubmodulePaths(const GitClient& client) {
    std::error_code error;
    if (!std::filesystem::exists(client.Directory() / ".gitmodules", error)) return {};
    std::vector<std::string> paths;
    for (const auto& line :
         SplitGitLines(client.Run({"config", "-f", ".gitmodules", "--get-regexp", "^submodule\\..*\\.path$"}).output)) {
        const auto space = line.find(' ');
        if (space != std::string::npos) paths.push_back(line.substr(space + 1));
    }
    return paths;
}

bool GitSubmoduleInitialized(const GitClient& client, const std::string& path) {
    std::error_code error;
    return std::filesystem::exists(client.Directory() / FromUtf8(path) / ".git", error);
}

GitProblem GitFetch(const GitClient& client, GitAccess access, const std::atomic<bool>* cancel) {
    // Fetching only reads the remote; maintenance would race an agent's git and take the disk for no visible reason.
    const auto timeout =
        access == GitAccess::Background ? std::chrono::milliseconds(std::chrono::minutes(2)) : NoTimeout;
    return Checked(
        client.Run({"-c", "gc.auto=0", "-c", "maintenance.auto=false", "fetch", "--no-recurse-submodules", "--quiet"},
                   access, cancel, timeout));
}

GitProblem GitPull(const GitClient& client, const GitActionContext& context) {
    // Merge rather than `git pull`: a pull.rebase in the developer's settings must not change what this button does.
    return FetchedPull(client, context, {"merge", "--ff-only", "--no-autostash", "--no-edit", "@{upstream}"});
}

GitProblem GitStashAndPull(const GitClient& client, const GitActionContext& context, bool includeUntracked) {
    if (auto fetched = GitFetch(client, GitAccess::Interactive, context.cancel); !Fine(fetched)) return fetched;
    std::vector<std::string> stash{"stash", "push", "-m", std::string(StashMessage)};
    if (includeUntracked) stash.emplace_back("--include-untracked");
    const auto saved = Action(client, stash, context);
    if (!saved.ok) return ProblemOf(saved);
    const bool hasStash = saved.output.find("No local changes") == std::string::npos;
    const auto merged = Action(client, {"merge", "--ff-only", "--no-autostash", "--no-edit", "@{upstream}"}, context);
    if (hasStash) {
        const auto restored = Action(client, {"stash", "pop"}, context);
        if (!restored.ok) {
            auto problem = ProblemOf(restored);
            problem.kind = GitProblemKind::Conflicts;
            problem.title = "Your changes conflict with the pulled ones. A copy stays in the stash until you drop it.";
            return problem;
        }
    }
    if (!merged.ok) return ProblemOf(merged);
    return GitSyncSubmodules(client, context);
}

GitProblem GitMergeIncoming(const GitClient& client, const GitActionContext& context) {
    return FetchedPull(client, context, {"merge", "--no-autostash", "--no-edit", "@{upstream}"});
}

GitProblem GitAbortSequence(const GitClient& client, GitSequence sequence) {
    const char* command = nullptr;
    switch (sequence) {
    case GitSequence::Merge     : command = "merge"; break;
    case GitSequence::Rebase    : command = "rebase"; break;
    case GitSequence::CherryPick: command = "cherry-pick"; break;
    case GitSequence::Revert    : command = "revert"; break;
    case GitSequence::Bisect    :
    case GitSequence::None      : return {};
    }
    return Checked(client.Run({command, "--abort"}, GitAccess::Interactive, nullptr, NoTimeout));
}

GitProblem GitSyncSubmodules(const GitClient& client, const GitActionContext& context) {
    std::vector<std::string> blocked;
    std::string detail;
    for (const auto& path : GitSubmodulePaths(client)) {
        const auto recorded = RecordedCommit(client, path);
        if (recorded.empty()) continue;
        GitSubmoduleFacts facts;
        facts.initialized = GitSubmoduleInitialized(client, path);
        const auto submodule = client.At(client.Directory() / FromUtf8(path));
        if (facts.initialized) {
            const auto head = FirstLine(submodule.Run({"rev-parse", "HEAD"}).output);
            facts.atRecorded = head == recorded;
            facts.onBranch = submodule.Run({"symbolic-ref", "-q", "HEAD"}).ok;
            facts.dirty = !submodule.Run({"status", "--porcelain", "--untracked-files=no"}).output.empty();
            if (!facts.atRecorded) {
                // The commit may not have reached this machine yet.
                if (!submodule.Run({"cat-file", "-e", recorded + "^{commit}"}).ok)
                    (void)GitFetch(submodule, GitAccess::Interactive, context.cancel);
                facts.recordedIsAhead = IsAncestor(submodule, head, recorded);
                facts.recordedIsBehind = IsAncestor(submodule, recorded, head);
            }
        }
        GitResult result;
        switch (DecideGitSubmoduleAction(facts)) {
        case GitSubmoduleAction::None: continue;
        case GitSubmoduleAction::Initialize:
            result = Action(client, {"submodule", "update", "--init", "--", path}, context);
            break;
        case GitSubmoduleAction::FastForwardBranch:
            // Stays on its branch: `submodule update` would detach HEAD and hide later commits.
            result = Action(submodule, {"merge", "--ff-only", "--no-autostash", recorded}, context);
            break;
        case GitSubmoduleAction::UpdateDetached:
            result = Action(client, {"submodule", "update", "--", path}, context);
            break;
        case GitSubmoduleAction::ReportDirty:
            blocked.push_back(path);
            detail += path + " has uncommitted changes.\n";
            continue;
        case GitSubmoduleAction::ReportDiverged:
            blocked.push_back(path);
            detail += path + " has commits the new version does not contain.\n";
            continue;
        }
        if (!result.ok) {
            blocked.push_back(path);
            detail += path + ": " + result.errors + "\n";
        }
    }
    if (blocked.empty()) return {};
    return Problem(GitProblemKind::SubmoduleNotUpdated, "Some submodules were left as they are.", std::move(detail),
                   std::move(blocked));
}

GitProblem GitCommit(const GitClient& client, const GitActionContext& context, const std::vector<std::string>& paths,
                     const std::string& message, bool allowLarge, GitSequence sequence) {
    if (sequence == GitSequence::Merge) {
        // A merge commits as a whole; `git commit <paths>` refuses during one.
        return message.empty() ? Checked(Action(client, {"commit", "--no-edit"}, context))
                               : Checked(Action(client, {"commit", "-m", message}, context));
    }
    if (paths.empty()) return Problem(GitProblemKind::Failed, "Select the files to commit.");
    if (message.empty()) return Problem(GitProblemKind::Failed, "Write a commit message.");
    if (!allowLarge) {
        auto large = LargeUnfilteredFiles(client, paths);
        if (!large.empty())
            return Problem(GitProblemKind::LargeFiles,
                           "These files are over 100 MB. Many hosts refuse them; Git LFS is the usual answer.", {},
                           std::move(large));
    }
    const PathspecFile list(context.scratch, paths);
    if (auto added = Checked(Action(client, {"add", "-A", list.Argument(), "--pathspec-file-nul"}, context));
        !Fine(added))
        return added;
    // Naming the paths keeps out whatever else someone staged, such as an agent working in the same folder.
    return Checked(Action(client, {"commit", "-m", message, list.Argument(), "--pathspec-file-nul"}, context));
}

GitProblem GitPush(const GitClient& client, const GitActionContext& context, const std::vector<std::string>& remotes) {
    std::vector<std::string> unpushed;
    for (const auto& path : GitSubmodulePaths(client)) {
        if (!GitSubmoduleInitialized(client, path)) continue;
        const auto recorded = RecordedCommit(client, path);
        if (recorded.empty()) continue;
        const auto submodule = client.At(client.Directory() / FromUtf8(path));
        if (SplitGitLines(submodule.Run({"remote"}).output).empty()) continue;
        if (submodule.Run({"branch", "-r", "--contains", recorded}).output.empty()) unpushed.push_back(path);
    }
    if (!unpushed.empty())
        return Problem(GitProblemKind::UnpushedSubmodule,
                       "This version records submodule commits only you have. Nobody else could build it until they "
                       "are pushed too.",
                       {}, std::move(unpushed));
    const auto upstream = client.Run({"rev-parse", "--abbrev-ref", "@{upstream}"});
    const auto upstreamExists = upstream.ok && client.Run({"rev-parse", "--verify", "@{upstream}"}).ok;
    if (upstreamExists) return Checked(Action(client, {"push"}, context));
    if (remotes.empty()) return Problem(GitProblemKind::Failed, "This repository has no remote to push to.");
    const auto remote = std::ranges::find(remotes, "origin") != remotes.end() ? std::string("origin") : remotes.front();
    return Checked(Action(client, {"push", "-u", remote, "HEAD"}, context));
}

GitProblem GitPushSubmodules(const GitClient& client, const GitActionContext& context) {
    for (const auto& path : GitSubmodulePaths(client)) {
        if (!GitSubmoduleInitialized(client, path)) continue;
        const auto recorded = RecordedCommit(client, path);
        if (recorded.empty()) continue;
        const auto submodule = client.At(client.Directory() / FromUtf8(path));
        if (SplitGitLines(submodule.Run({"remote"}).output).empty()) continue;
        if (!submodule.Run({"branch", "-r", "--contains", recorded}).output.empty()) continue;
        if (!submodule.Run({"symbolic-ref", "-q", "HEAD"}).ok)
            return Problem(GitProblemKind::Failed, path + " is not on a branch, so it cannot be pushed from here.");
        if (auto problem = GitPush(submodule, context, SplitGitLines(submodule.Run({"remote"}).output)); !Fine(problem))
            return problem;
    }
    return {};
}

GitProblem GitInitialize(const GitClient& client, const std::filesystem::path& templateDirectory) {
    auto created = client.Run({"init", "-b", "main"}, GitAccess::Interactive);
    if (!created.ok) created = client.Run({"init"}, GitAccess::Interactive); // Git before 2.28
    if (!created.ok) return ProblemOf(created);
    WriteTemplateFile(templateDirectory, client.Directory() / ".gitignore", "gitignore", "build/\nRelease/\n");
    WriteTemplateFile(templateDirectory, client.Directory() / ".gitattributes", "gitattributes", "* text=auto\n");
    return {};
}

GitProblem GitAddRemote(const GitClient& client, const std::string& url) {
    if (!GitSafeArgument(url)) return Problem(GitProblemKind::Failed, "That is not a repository address.");
    return Checked(client.Run({"remote", "add", "origin", url}, GitAccess::Interactive));
}

GitProblem GitSwitchBranch(const GitClient& client, const std::string& branch) {
    if (!GitSafeArgument(branch)) return Problem(GitProblemKind::Failed, "That is not a branch name.");
    // Commits on no branch would become hard to find again.
    const bool detached = !client.Run({"symbolic-ref", "-q", "HEAD"}).ok;
    if (detached && client.Run({"rev-parse", "--verify", "HEAD"}).ok &&
        SplitGitLines(client.Run({"branch", "-a", "--contains", "HEAD"}).output).size() <= 1)
        return Problem(GitProblemKind::Failed,
                       "The current commit is on no branch. Create a branch here first, or it will be hard to find.");
    return Checked(client.Run({"switch", branch}, GitAccess::Interactive, nullptr, NoTimeout));
}

GitProblem GitCreateBranch(const GitClient& client, const std::string& branch) {
    if (!GitSafeArgument(branch) || !client.Run({"check-ref-format", "--branch", branch}).ok)
        return Problem(GitProblemKind::Failed, "That is not a valid branch name.");
    return Checked(client.Run({"switch", "-c", branch}, GitAccess::Interactive));
}

GitProblem GitSetIdentity(const GitClient& client, const std::string& name, const std::string& email) {
    if (!GitSafeArgument(name) || !GitSafeArgument(email))
        return Problem(GitProblemKind::Failed, "Enter both a name and an email.");
    if (auto problem = Checked(client.Run({"config", "--global", "user.name", name}, GitAccess::Interactive));
        !Fine(problem))
        return problem;
    return Checked(client.Run({"config", "--global", "user.email", email}, GitAccess::Interactive));
}

GitProblem GitTakeConflictSide(const GitClient& client, const GitChange& change, GitConflictSide side,
                               GitSequence sequence) {
    // In a merge "ours" is the developer's side. Restoring a stash has no sequence and is the reverse: the pulled
    // commit is ours, the saved changes are theirs.
    const bool mineIsOurs = sequence != GitSequence::None;
    const bool useOurs = (side == GitConflictSide::Mine) == mineIsOurs;
    const char letter = useOurs ? change.index : change.worktree;
    if (letter == 'D') // That side deleted the file
        return Checked(client.Run({"rm", "--force", "--", change.path}, GitAccess::Interactive));
    if (auto problem = Checked(
            client.Run({"checkout", useOurs ? "--ours" : "--theirs", "--", change.path}, GitAccess::Interactive));
        !Fine(problem))
        return problem;
    return GitMarkResolved(client, change.path);
}

GitProblem GitMarkResolved(const GitClient& client, const std::string& path) {
    return Checked(client.Run({"add", "-A", "--", path}, GitAccess::Interactive));
}

GitProblem GitIgnorePath(const GitClient& client, const std::string& path) {
    const auto file = client.Directory() / ".gitignore";
    auto text = ReadWholeFile(file);
    if (!text.empty() && text.back() != '\n') text += '\n';
    // Anchored at the root, so only this one file or folder is ignored; '!', '#' and the like read as plain text.
    std::string pattern = "/";
    for (const char character : path) {
        if (character == '*' || character == '?' || character == '[' || character == '\\') pattern += '\\';
        pattern += character;
    }
    if (text.find(pattern + "\n") != std::string::npos) return {};
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text << pattern << '\n';
    if (!out) return Problem(GitProblemKind::Failed, "Could not write .gitignore.");
    return {};
}

GitProblem GitRemoveLock(const GitClient& client) {
    const auto directory = client.Run({"rev-parse", "--absolute-git-dir"});
    if (!directory.ok) return ProblemOf(directory);
    std::error_code error;
    std::filesystem::remove(FromUtf8(FirstLine(directory.output)) / "index.lock", error);
    if (error) return Problem(GitProblemKind::Failed, "Could not remove the lock file.", error.message());
    return {};
}

GitProblem GitTrustDirectory(const GitClient& client, const std::string& problemDetail) {
    // Git's message ends with the exact command: git config --global --add safe.directory '<folder>'
    constexpr std::string_view Marker = "safe.directory ";
    std::string folder;
    if (const auto marker = problemDetail.rfind(Marker); marker != std::string::npos) {
        folder = FirstLine(problemDetail.substr(marker + Marker.size()));
        std::erase(folder, '\'');
    }
    if (folder.empty()) {
        const auto text = client.Directory().generic_u8string();
        folder.assign(reinterpret_cast<const char*>(text.data()), text.size());
    }
    return Checked(client.Run({"config", "--global", "--add", "safe.directory", folder}, GitAccess::Interactive));
}

std::string GitPathInside(const std::filesystem::path& folder, const std::filesystem::path& repository) {
    std::error_code error;
    const auto relative = std::filesystem::relative(folder, repository, error);
    if (error || relative.empty() || *relative.begin() == "..") return {};
    const auto text = relative.generic_u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size()) + "/";
}

GitDiff ReadGitDiff(const GitClient& client, const GitChange& change, const GitActionContext& context) {
    GitDiff diff;
    diff.path = change.path;
    if (change.path.ends_with('/')) {
        diff.kind = GitDiffKind::Folder;
        return diff;
    }
    const auto preview = change.submodule ? GitDiffKind::Text : GitPreviewKind(change.path);
    if (preview != GitDiffKind::Text) {
        diff.kind = preview;
        std::error_code error;
        const auto after = client.Directory() / FromUtf8(change.path);
        if (std::filesystem::is_regular_file(after, error)) diff.after = after;
        const auto& source = change.originalPath.empty() ? change.path : change.originalPath;
        // With filters, so an LFS file comes back as the image and not as its pointer.
        const auto old = client.Run({"cat-file", "--filters", "HEAD:" + source});
        if (old.ok) {
            std::filesystem::create_directories(context.scratch, error);
            auto name = std::filesystem::path("before-" + std::to_string(std::hash<std::string>{}(source)));
            name += FromUtf8(change.path).extension();
            const auto before = context.scratch / name;
            std::ofstream(before, std::ios::binary) << old.output;
            diff.before = before;
        }
        return diff;
    }
    GitResult result;
    if (change.kind == GitChangeKind::Untracked)
        // Exit code 1 only means the files differ.
        result = client.Run({"diff", "--no-index", "--no-ext-diff", "--", "/dev/null", change.path});
    else if (client.Run({"rev-parse", "--verify", "HEAD"}).ok)
        result = client.Run({"diff", "--no-ext-diff", "--find-renames", "HEAD", "--", change.path});
    else
        result = client.Run({"diff", "--no-ext-diff", "--cached", "--", change.path});
    if (result.output.find("Binary files ") != std::string::npos && result.output.find("\n@@") == std::string::npos) {
        diff.kind = GitDiffKind::Binary;
        return diff;
    }
    diff.kind = GitDiffKind::Text;
    diff.lines = ParseGitDiffLines(result.output, GitMaximumDiffLines, diff.truncated);
    return diff;
}
