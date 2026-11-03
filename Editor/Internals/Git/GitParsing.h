#pragma once
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Git/GitTypes.h"

// Pure readers of git's machine output and of its English error text (the client forces the C locale).
[[nodiscard]] GitStatus ParseGitStatus(std::string_view porcelainV2);
// `git log --format=%h%x1f%s%x1f%an%x1f%ct%x1e`
[[nodiscard]] std::vector<GitCommitSummary> ParseGitLog(std::string_view text);
// `git diff --raw -z --no-renames --no-abbrev`
[[nodiscard]] std::vector<GitDiffPath> ParseGitRawDiff(std::string_view text);
[[nodiscard]] std::vector<std::string> SplitGitNul(std::string_view text);
[[nodiscard]] std::vector<std::string> SplitGitLines(std::string_view text);
// `git check-attr filter -z`: the paths whose filter is lfs.
[[nodiscard]] std::vector<std::string> ParseGitLfsPaths(std::string_view checkAttr);
// The failure as the developer should read it; `detail` keeps git's own words.
[[nodiscard]] GitProblem ClassifyGitError(std::string_view errors);
// The file list git prints under "would be overwritten by merge" and similar.
[[nodiscard]] std::vector<std::string> ParseGitBlockedFiles(std::string_view errors);
// Files present in `incoming` that also changed here; an untracked folder covers what it contains.
[[nodiscard]] std::vector<std::string> GitOverlap(const std::vector<GitChange>& changes,
                                                  const std::vector<std::string>& incoming);
[[nodiscard]] GitStep NextGitStep(const GitRepositoryState& repository);
[[nodiscard]] std::vector<GitDiffLine> ParseGitDiffLines(std::string_view text, std::size_t maximum, bool& truncated);
[[nodiscard]] GitDiffKind GitPreviewKind(std::string_view path);
// Whether `name` may be typed as a remote URL or branch: git must not read it as an option.
[[nodiscard]] bool GitSafeArgument(std::string_view name);

// What to do with one submodule whose recorded commit may have moved. Local work is never overwritten: a branch moves
// only forward, and a detached checkout only when nothing would be left behind.
enum class GitSubmoduleAction { None, Initialize, FastForwardBranch, UpdateDetached, ReportDirty, ReportDiverged };
struct GitSubmoduleFacts {
    bool initialized = false;
    bool dirty = false;
    bool onBranch = false;
    bool atRecorded = false;
    bool recordedIsAhead = false;  // HEAD is an ancestor of the recorded commit
    bool recordedIsBehind = false; // The recorded commit is an ancestor of HEAD: local commits not recorded yet
};
[[nodiscard]] GitSubmoduleAction DecideGitSubmoduleAction(const GitSubmoduleFacts& facts);
