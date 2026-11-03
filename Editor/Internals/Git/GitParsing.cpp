#include "Editor/Internals/Git/GitParsing.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>

namespace {
constexpr char UnitSeparator = '\x1f';
constexpr char RecordSeparator = '\x1e';
std::vector<std::string_view> Fields(std::string_view line, std::size_t count) {
    // The last field keeps the rest of the line: a path may hold spaces.
    std::vector<std::string_view> fields;
    while (fields.size() + 1 < count) {
        const auto space = line.find(' ');
        if (space == std::string_view::npos) break;
        fields.push_back(line.substr(0, space));
        line.remove_prefix(space + 1);
    }
    fields.push_back(line);
    return fields;
}
int Number(std::string_view text) {
    int value = 0;
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) text.remove_prefix(1);
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}
GitChangeKind KindOf(char index, char worktree) {
    const char letter = index != '.' ? index : worktree;
    switch (letter) {
    case 'A': return GitChangeKind::Added;
    case 'D': return GitChangeKind::Deleted;
    case 'R': return GitChangeKind::Renamed;
    case 'C': return GitChangeKind::Copied;
    case 'T': return GitChangeKind::TypeChanged;
    default : return GitChangeKind::Modified;
    }
}
bool Contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }
std::string Trimmed(std::string_view text) {
    const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!text.empty() && space(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && space(text.back()))
        text.remove_suffix(1);
    return std::string(text);
}
std::string Lower(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
} // namespace

std::vector<std::string> SplitGitNul(std::string_view text) {
    std::vector<std::string> parts;
    while (!text.empty()) {
        const auto end = text.find('\0');
        if (end == std::string_view::npos) {
            parts.emplace_back(text);
            break;
        }
        parts.emplace_back(text.substr(0, end));
        text.remove_prefix(end + 1);
    }
    return parts;
}

std::vector<std::string> SplitGitLines(std::string_view text) {
    std::vector<std::string> lines;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (!line.empty()) lines.emplace_back(line);
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return lines;
}

GitStatus ParseGitStatus(std::string_view porcelainV2) {
    GitStatus status;
    const auto entries = SplitGitNul(porcelainV2);
    bool hasCounts = false;
    for (std::size_t at = 0; at < entries.size(); ++at) {
        const std::string_view entry = entries[at];
        if (entry.size() < 2) continue;
        if (entry.starts_with("# ")) {
            const auto fields = Fields(entry, 3);
            if (fields.size() < 3) continue;
            if (fields[1] == "branch.oid" && fields[2] != "(initial)") status.branch.oid = fields[2];
            if (fields[1] == "branch.head" && fields[2] != "(detached)") status.branch.head = fields[2];
            if (fields[1] == "branch.upstream") status.branch.upstream = fields[2];
            if (fields[1] == "branch.ab") {
                const auto counts = Fields(fields[2], 2);
                if (counts.size() == 2) {
                    status.branch.ahead = Number(counts[0]);
                    status.branch.behind = Number(counts[1]);
                    hasCounts = true;
                }
            }
            continue;
        }
        GitChange change;
        switch (entry.front()) {
        case '1': { // 1 XY sub mH mI mW hH hI path
            const auto fields = Fields(entry, 9);
            if (fields.size() < 9 || fields[1].size() != 2) continue;
            change.index = fields[1][0];
            change.worktree = fields[1][1];
            change.kind = KindOf(change.index, change.worktree);
            change.submodule = fields[2].starts_with('S');
            change.path = fields[8];
            break;
        }
        case '2': { // 2 XY sub mH mI mW hH hI Xscore path, then the original path as its own entry
            const auto fields = Fields(entry, 10);
            if (fields.size() < 10 || fields[1].size() != 2) continue;
            change.index = fields[1][0];
            change.worktree = fields[1][1];
            change.kind = KindOf(change.index, change.worktree);
            change.submodule = fields[2].starts_with('S');
            change.path = fields[9];
            if (at + 1 < entries.size()) change.originalPath = entries[++at];
            break;
        }
        case 'u': { // u XY sub m1 m2 m3 mW h1 h2 h3 path
            const auto fields = Fields(entry, 11);
            if (fields.size() < 11 || fields[1].size() != 2) continue;
            change.index = fields[1][0];
            change.worktree = fields[1][1];
            change.kind = GitChangeKind::Conflicted;
            change.submodule = fields[2].starts_with('S');
            change.path = fields[10];
            break;
        }
        case '?':
            change.kind = GitChangeKind::Untracked;
            change.index = '?';
            change.worktree = '?';
            change.path = entry.substr(2);
            break;
        default: continue;
        }
        status.changes.push_back(std::move(change));
    }
    status.branch.upstreamGone = !status.branch.upstream.empty() && !hasCounts;
    return status;
}

std::vector<GitCommitSummary> ParseGitLog(std::string_view text) {
    std::vector<GitCommitSummary> commits;
    while (!text.empty()) {
        const auto end = text.find(RecordSeparator);
        auto record = text.substr(0, end);
        while (!record.empty() && (record.front() == '\n' || record.front() == '\r'))
            record.remove_prefix(1);
        std::array<std::string_view, 4> fields{};
        std::size_t count = 0;
        while (count < fields.size()) {
            const auto separator = record.find(UnitSeparator);
            fields[count++] = record.substr(0, separator);
            if (separator == std::string_view::npos) break;
            record.remove_prefix(separator + 1);
        }
        if (count == fields.size()) {
            std::int64_t time = 0;
            std::from_chars(fields[3].data(), fields[3].data() + fields[3].size(), time);
            commits.push_back({std::string(fields[0]), std::string(fields[1]), std::string(fields[2]), time});
        }
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return commits;
}

std::vector<GitDiffPath> ParseGitRawDiff(std::string_view text) {
    constexpr std::string_view SubmoduleMode = "160000";
    std::vector<GitDiffPath> paths;
    const auto entries = SplitGitNul(text);
    for (std::size_t at = 0; at + 1 < entries.size(); at += 2) {
        const std::string_view meta = entries[at];
        if (!meta.starts_with(':')) continue;
        const auto fields = Fields(meta.substr(1), 2);
        const bool oldSubmodule = !fields.empty() && fields[0] == SubmoduleMode;
        const bool newSubmodule = fields.size() > 1 && fields[1].starts_with(SubmoduleMode);
        paths.push_back({entries[at + 1], oldSubmodule || newSubmodule});
    }
    return paths;
}

std::vector<std::string> ParseGitLfsPaths(std::string_view checkAttr) {
    std::vector<std::string> result;
    const auto fields = SplitGitNul(checkAttr);
    for (std::size_t at = 0; at + 2 < fields.size(); at += 3)
        if (fields[at + 2] == "lfs") result.push_back(fields[at]);
    return result;
}

GitProblem ClassifyGitError(std::string_view errors) {
    GitProblem problem;
    problem.detail = Trimmed(errors);
    const auto text = Lower(errors);
    const auto has = [&](std::string_view part) { return Contains(text, part); };
    const auto set = [&](GitProblemKind kind, const char* title) {
        problem.kind = kind;
        problem.title = title;
    };
    if (has("detected dubious ownership"))
        set(GitProblemKind::DubiousOwnership, "Git does not trust this folder yet.");
    else if (has("index.lock") || has("another git process seems to be running"))
        set(GitProblemKind::Locked, "Another git process holds this repository.");
    else if (has("would be overwritten by") || has("please commit your changes or stash"))
        set(GitProblemKind::LocalChanges, "Your local changes overlap the incoming ones.");
    else if (has("git-lfs") || has("git lfs") || has("configured for git lfs"))
        set(GitProblemKind::LfsMissing, "This repository needs Git LFS.");
    else if (has("please tell me who you are") || has("unable to auto-detect email") || has("empty ident name"))
        set(GitProblemKind::Identity, "Git needs your name and email for commits.");
    else if (has("could not resolve host") || has("failed to connect") || has("connection timed out") ||
             has("network is unreachable") || has("connection refused") || has("timed out"))
        set(GitProblemKind::Offline, "The remote cannot be reached.");
    else if (has("authentication failed") || has("permission denied") || has("could not read username") ||
             has("could not read password") || has("terminal prompts disabled") || has("error: 403") ||
             has("repository not found") || has("host key verification failed") ||
             has("could not read from remote repository"))
        set(GitProblemKind::Authentication, "Could not sign in to the remote.");
    else if (has("unable to access"))
        set(GitProblemKind::Offline, "The remote cannot be reached.");
    else if (has("non-fast-forward") || has("fetch first") || has("[rejected]"))
        set(GitProblemKind::PushRejected, "The remote has commits you do not have. Pull first.");
    else if (has("not possible to fast-forward") || has("diverging branches"))
        set(GitProblemKind::Diverged, "This branch and the remote have both moved.");
    else if (has("conflict") || has("unmerged files") || has("you have unmerged paths"))
        set(GitProblemKind::Conflicts, "Some files conflict.");
    else
        set(GitProblemKind::Failed, "Git could not finish this.");
    problem.files = ParseGitBlockedFiles(errors);
    return problem;
}

std::vector<std::string> ParseGitBlockedFiles(std::string_view errors) {
    std::vector<std::string> files;
    bool listing = false;
    for (const auto& line : SplitGitLines(errors)) {
        if (Contains(Lower(line), "would be overwritten by")) {
            listing = true;
            continue;
        }
        if (!listing) continue;
        if (line.front() == '\t' || line.front() == ' ')
            files.push_back(Trimmed(line));
        else
            listing = false;
    }
    return files;
}

std::vector<std::string> GitOverlap(const std::vector<GitChange>& changes, const std::vector<std::string>& incoming) {
    std::vector<std::string> overlap;
    for (const auto& file : incoming) {
        const bool found = std::ranges::any_of(changes, [&](const GitChange& change) {
            return change.path == file || (change.path.ends_with('/') && file.starts_with(change.path));
        });
        if (found) overlap.push_back(file);
    }
    return overlap;
}

GitStep NextGitStep(const GitRepositoryState& repository) {
    const bool conflicts = std::ranges::any_of(
        repository.changes, [](const GitChange& change) { return change.kind == GitChangeKind::Conflicted; });
    if (conflicts) return GitStep::Resolve;
    if (repository.sequence == GitSequence::Merge) return GitStep::Commit;
    if (repository.branch.behind > 0) return GitStep::Pull;
    if (!repository.changes.empty()) return GitStep::Commit;
    if (repository.branch.ahead > 0) return GitStep::Push;
    // A branch the remote has not seen yet. A project with no remote at all is a fine place to stay: no nagging.
    if (!repository.branch.oid.empty() && repository.branch.upstream.empty() && !repository.branch.head.empty() &&
        !repository.remotes.empty())
        return GitStep::Push;
    return GitStep::None;
}

std::vector<GitDiffLine> ParseGitDiffLines(std::string_view text, std::size_t maximum, bool& truncated) {
    std::vector<GitDiffLine> lines;
    truncated = false;
    bool inHunk = false;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.starts_with("diff ")) inHunk = false; // File headers again
        if (line.starts_with("@@")) {
            inHunk = true;
            lines.push_back({GitDiffLineKind::Hunk, std::string(line)});
        } else if (inHunk) {
            const char first = line.empty() ? ' ' : line.front();
            const auto kind = first == '+'   ? GitDiffLineKind::Added
                              : first == '-' ? GitDiffLineKind::Removed
                                             : GitDiffLineKind::Context;
            if (first != '\\') lines.push_back({kind, std::string(line)});
        }
        if (lines.size() >= maximum) {
            truncated = true;
            break;
        }
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return lines;
}

GitDiffKind GitPreviewKind(std::string_view path) {
    const auto dot = path.rfind('.');
    if (dot == std::string_view::npos) return GitDiffKind::Text;
    const auto extension = Lower(path.substr(dot));
    for (const char* image : {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif"})
        if (extension == image) return GitDiffKind::Image;
    for (const char* audio : {".wav", ".ogg", ".mp3", ".flac"})
        if (extension == audio) return GitDiffKind::Audio;
    return GitDiffKind::Text;
}

bool GitSafeArgument(std::string_view name) {
    return !name.empty() && name.front() != '-' &&
           std::ranges::none_of(name, [](char c) { return c == '\0' || c == '\n' || c == '\r'; });
}

GitSubmoduleAction DecideGitSubmoduleAction(const GitSubmoduleFacts& facts) {
    if (!facts.initialized) return GitSubmoduleAction::Initialize;
    if (facts.atRecorded) return GitSubmoduleAction::None;
    if (facts.recordedIsBehind) return GitSubmoduleAction::None; // Developing inside it: commits not recorded yet
    if (facts.dirty) return GitSubmoduleAction::ReportDirty;
    if (!facts.recordedIsAhead) return GitSubmoduleAction::ReportDiverged;
    return facts.onBranch ? GitSubmoduleAction::FastForwardBranch : GitSubmoduleAction::UpdateDetached;
}
