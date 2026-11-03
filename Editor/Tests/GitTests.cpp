#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include "Editor/Internals/Git/GitAttention.h"
#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Git/GitOperations.h"
#include "Editor/Internals/Git/GitParsing.h"
#include "Editor/Internals/Git/GitSchedule.h"
#include "Editor/Internals/Git/GitService.h"
#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include "Tests/Support/TempDirectory.h"

using namespace std::string_literals;

namespace {
void Write(const std::filesystem::path& file, const std::string& text) {
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
}
std::string Read(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
// Real repositories in a temporary folder: a remote, this machine, and a teammate.
class Sandbox {
public:
    Sandbox()
        : _git(FindGitExecutable()) {}
    [[nodiscard]] bool HasGit() const { return !_git.empty(); }
    [[nodiscard]] GitClient At(const std::filesystem::path& folder) const { return {_git, folder}; }
    void Must(const std::filesystem::path& folder, const std::vector<std::string>& arguments) const {
        const auto result = At(folder).Run(arguments);
        INFO(result.errors);
        REQUIRE(result.ok);
    }
    void Configure(const std::filesystem::path& folder) const {
        Must(folder, {"config", "user.name", "Tester"});
        Must(folder, {"config", "user.email", "tester@example.com"});
        Must(folder, {"config", "core.autocrlf", "false"});
    }
    // Remote with one commit; `local` and `teammate` cloned from it.
    void Start() {
        std::filesystem::create_directories(Remote());
        Must(Remote(), {"init", "--bare", "-b", "main"});
        Must(Root(), {"clone", Remote().generic_string(), "local"});
        Configure(Local());
        Must(Local(), {"switch", "-c", "main"});
        Write(Local() / "readme.txt", "one\n");
        Must(Local(), {"add", "-A"});
        Must(Local(), {"commit", "-m", "First"});
        Must(Local(), {"push", "-u", "origin", "main"});
        // Line endings as written, whatever this machine's git prefers; otherwise every file reads as changed.
        Must(Root(), {"clone", "-c", "core.autocrlf=false", Remote().generic_string(), "teammate"});
        Configure(Teammate());
    }
    void TeammateCommits(const std::string& file, const std::string& text) const {
        Write(Teammate() / file, text);
        Must(Teammate(), {"add", "-A"});
        Must(Teammate(), {"commit", "-m", "Change " + file});
        Must(Teammate(), {"push"});
    }
    [[nodiscard]] GitRepositoryState State() {
        Must(Local(), {"fetch"});
        auto state = ReadGitRepository(At(Local()), _cache, true);
        REQUIRE(state.has_value());
        return *state;
    }
    [[nodiscard]] GitActionContext Context() const { return {nullptr, Root() / "scratch"}; }
    [[nodiscard]] std::filesystem::path Root() const { return _folder.Path(); }
    [[nodiscard]] std::filesystem::path Remote() const { return Root() / "remote.git"; }
    [[nodiscard]] std::filesystem::path Local() const { return Root() / "local"; }
    [[nodiscard]] std::filesystem::path Teammate() const { return Root() / "teammate"; }
    [[nodiscard]] std::filesystem::path Executable() const { return _git; }

private:
    TempDirectory _folder;
    std::filesystem::path _git;
    GitRepositoryCache _cache;
};
bool Fine(const GitProblem& problem) { return problem.kind == GitProblemKind::None; }
} // namespace

TEST_CASE("Editor.Git.ParsesStatusEntries", "[engine-editor]") {
    const auto text = "# branch.oid abc123\0# branch.head main\0# branch.upstream origin/main\0# branch.ab +2 -3\0"
                      "1 .M N... 100644 100644 100644 aaa aaa my file.txt\0"
                      "2 R. N... 100644 100644 100644 aaa bbb R100 new name.txt\0old name.txt\0"
                      "u UU N... 100644 100644 100644 100644 a b c both.txt\0"
                      "1 .M SC.. 160000 160000 160000 aaa aaa Engine\0"
                      "? art/\0"s;
    const auto status = ParseGitStatus(text);
    CHECK(status.branch.head == "main");
    CHECK(status.branch.oid == "abc123");
    CHECK(status.branch.ahead == 2);
    CHECK(status.branch.behind == 3);
    CHECK_FALSE(status.branch.upstreamGone);
    REQUIRE(status.changes.size() == 5);
    CHECK(status.changes[0].path == "my file.txt");
    CHECK(status.changes[0].kind == GitChangeKind::Modified);
    CHECK(status.changes[1].path == "new name.txt");
    CHECK(status.changes[1].originalPath == "old name.txt");
    CHECK(status.changes[1].kind == GitChangeKind::Renamed);
    CHECK(status.changes[2].kind == GitChangeKind::Conflicted);
    CHECK(status.changes[3].submodule);
    CHECK(status.changes[4].kind == GitChangeKind::Untracked);
    CHECK(status.changes[4].path == "art/");
}

TEST_CASE("Editor.Git.ParsesDetachedAndUnbornBranches", "[engine-editor]") {
    CHECK(ParseGitStatus("# branch.oid abc\0# branch.head (detached)\0"s).branch.head.empty());
    CHECK(ParseGitStatus("# branch.oid (initial)\0# branch.head main\0"s).branch.oid.empty());
    CHECK(ParseGitStatus("# branch.oid abc\0# branch.head main\0# branch.upstream origin/main\0"s).branch.upstreamGone);
}

TEST_CASE("Editor.Git.ClassifiesFailures", "[engine-editor]") {
    const auto kind = [](const char* text) { return ClassifyGitError(text).kind; };
    CHECK(kind("fatal: unable to access 'https://x/': Could not resolve host: x") == GitProblemKind::Offline);
    CHECK(kind("remote: Invalid username or password.\nfatal: Authentication failed for 'https://x/'") ==
          GitProblemKind::Authentication);
    CHECK(kind("fatal: could not read Username for 'https://x': terminal prompts disabled") ==
          GitProblemKind::Authentication);
    CHECK(kind("error: failed to push some refs\n ! [rejected] main -> main (fetch first)") ==
          GitProblemKind::PushRejected);
    CHECK(kind("fatal: Unable to create 'C:/x/.git/index.lock': File exists.") == GitProblemKind::Locked);
    CHECK(kind("fatal: detected dubious ownership in repository at 'C:/x'") == GitProblemKind::DubiousOwnership);
    CHECK(kind("git-lfs filter-process: git-lfs: command not found") == GitProblemKind::LfsMissing);
    CHECK(kind("Author identity unknown\n*** Please tell me who you are.") == GitProblemKind::Identity);
    CHECK(kind("something nobody has seen") == GitProblemKind::Failed);
    const auto blocked = ClassifyGitError("error: Your local changes to the following files would be overwritten by "
                                          "merge:\n\ta.txt\n\tdir/b.txt\nPlease commit your changes or stash them.");
    CHECK(blocked.kind == GitProblemKind::LocalChanges);
    CHECK(blocked.files == std::vector<std::string>{"a.txt", "dir/b.txt"});
}

TEST_CASE("Editor.Git.NextStepFollowsTheWorkflow", "[engine-editor]") {
    GitRepositoryState state;
    state.branch.head = "main";
    state.branch.oid = "abc";
    state.branch.upstream = "origin/main";
    CHECK(NextGitStep(state) == GitStep::None);
    state.branch.ahead = 1;
    CHECK(NextGitStep(state) == GitStep::Push);
    state.changes.push_back({.path = "a", .kind = GitChangeKind::Modified});
    CHECK(NextGitStep(state) == GitStep::Commit);
    state.branch.behind = 1;
    CHECK(NextGitStep(state) == GitStep::Pull);
    state.changes.push_back({.path = "b", .kind = GitChangeKind::Conflicted});
    CHECK(NextGitStep(state) == GitStep::Resolve);
}

TEST_CASE("Editor.Git.SubmoduleDecisionsNeverLoseWork", "[engine-editor]") {
    const auto decide = [](GitSubmoduleFacts facts) { return DecideGitSubmoduleAction(facts); };
    CHECK(decide({.initialized = false}) == GitSubmoduleAction::Initialize);
    CHECK(decide({.initialized = true, .atRecorded = true}) == GitSubmoduleAction::None);
    CHECK(decide({.initialized = true, .recordedIsBehind = true}) == GitSubmoduleAction::None);
    CHECK(decide({.initialized = true, .dirty = true, .recordedIsAhead = true}) == GitSubmoduleAction::ReportDirty);
    CHECK(decide({.initialized = true}) == GitSubmoduleAction::ReportDiverged);
    CHECK(decide({.initialized = true, .onBranch = true, .recordedIsAhead = true}) ==
          GitSubmoduleAction::FastForwardBranch);
    CHECK(decide({.initialized = true, .recordedIsAhead = true}) == GitSubmoduleAction::UpdateDetached);
}

TEST_CASE("Editor.Git.ScheduleFetchesOnOpenThenByTimeAndFocus", "[engine-editor]") {
    GitSchedule schedule;
    CHECK(schedule.FetchDue(0));
    schedule.FetchFinished(0);
    CHECK_FALSE(schedule.FetchDue(GitSchedule::FetchInterval - 1));
    CHECK(schedule.FetchDue(GitSchedule::FetchInterval));
    schedule.FetchFinished(1000);
    // Coming back soon does not fetch; coming back later does.
    schedule.SetFocused(false, 1001);
    schedule.SetFocused(true, 1000 + GitSchedule::FocusFetchInterval - 1);
    CHECK_FALSE(schedule.FetchDue(1000 + GitSchedule::FocusFetchInterval - 1));
    schedule.SetFocused(false, 1200);
    schedule.SetFocused(true, 1000 + GitSchedule::FocusFetchInterval + 1);
    CHECK(schedule.FetchDue(1000 + GitSchedule::FocusFetchInterval + 1));
}

TEST_CASE("Editor.Git.RefusedSignInStopsRetryingUntilAsked", "[engine-editor]") {
    GitSchedule schedule;
    schedule.FetchFinished(0);
    schedule.PauseFetching();
    CHECK_FALSE(schedule.FetchDue(GitSchedule::FetchInterval * 10));
    schedule.ResumeFetching();
    CHECK(schedule.FetchDue(GitSchedule::FetchInterval * 10));
}

TEST_CASE("Editor.Git.SlowStatusBacksOffAndNothingSpinsWhileIdle", "[engine-editor]") {
    GitSchedule schedule;
    schedule.FetchFinished(0);
    schedule.RefreshFinished(0, 2.0);
    CHECK_FALSE(schedule.RefreshDue(GitSchedule::RefreshInterval));
    CHECK(schedule.RefreshDue(2.0 * GitSchedule::RefreshCostFactor));
    CHECK(schedule.WaitSeconds(1) > 0);
    // Without git or a repository nothing may be due, or the service would spin.
    GitSchedule impossible;
    impossible.SetFetchPossible(false);
    impossible.RefreshFinished(0, 0);
    CHECK(impossible.WaitSeconds(1) > 0);
}

TEST_CASE("Editor.Git.PullPulsesBrieflyAndAgainForMore", "[engine-editor]") {
    GitAttention attention;
    attention.Update(0, 0);
    CHECK_FALSE(attention.Pulsing(0));
    attention.Update(3, 100);
    CHECK(attention.Pulsing(100 + GitAttention::PulseSeconds - 1));
    CHECK_FALSE(attention.Pulsing(100 + GitAttention::PulseSeconds));
    attention.Update(3, 200);
    CHECK_FALSE(attention.Pulsing(200));
    attention.Update(5, 300);
    CHECK(attention.Pulsing(300));
    attention.Update(0, 310);
    CHECK_FALSE(attention.Pulsing(310));
}

TEST_CASE("Editor.Git.BuildFolderIsRecognizedInsideTheRepository", "[engine-editor]") {
    const std::filesystem::path repository = "C:/Projects/MyProject";
    CHECK(GitPathInside("C:/Projects/MyProject/build", repository) == "build/");
    CHECK(GitPathInside("C:/Projects/MyProject/Out/Debug", repository) == "Out/Debug/");
    CHECK(GitPathInside("D:/Elsewhere/build", repository).empty());
    CHECK(GitPathInside("C:/Projects/build", repository).empty());
}

TEST_CASE("Editor.Git.BackgroundRunsCannotPrompt", "[engine-editor]") {
    const auto background = GitEnvironment(GitAccess::Background);
    CHECK(background.at("GIT_TERMINAL_PROMPT") == "0");
    CHECK(background.at("GCM_INTERACTIVE") == "never");
    CHECK(background.at("GIT_OPTIONAL_LOCKS") == "0");
    CHECK_FALSE(GitEnvironment(GitAccess::Interactive).contains("GCM_INTERACTIVE"));
}

TEST_CASE("Editor.Git.CommitTakesOnlyTheSelectedFiles", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    Write(sandbox.Local() / "picked.txt", "mine\n");
    Write(sandbox.Local() / "with space.txt", "mine\n");
    Write(sandbox.Local() / "[odd].txt", "mine\n"); // Means a character class unless pathspecs are literal
    Write(sandbox.Local() / "other.txt", "someone else's\n");
    // Someone (an agent) staged a file of their own.
    sandbox.Must(sandbox.Local(), {"add", "other.txt"});
    const auto client = sandbox.At(sandbox.Local());
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"picked.txt", "with space.txt", "[odd].txt"}, "Add picked",
                           false, GitSequence::None)));
    const auto committed = client.Run({"show", "--name-only", "--format=", "HEAD"}).output;
    CHECK(committed.find("picked.txt") != std::string::npos);
    CHECK(committed.find("with space.txt") != std::string::npos);
    CHECK(committed.find("other.txt") == std::string::npos);
    // Theirs is still staged, as they left it.
    CHECK(client.Run({"diff", "--cached", "--name-only"}).output.find("other.txt") != std::string::npos);
}

TEST_CASE("Editor.Git.CommitHandlesNewFoldersRenamesAndDeletions", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    Write(sandbox.Local() / "Art" / "Hero Sprites" / "hero.png", "x");
    Write(sandbox.Local() / "gone.txt", "bye\n");
    Write(sandbox.Local() / "moved.txt", "some text that stays the same\n");
    const auto client = sandbox.At(sandbox.Local());
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"gone.txt", "moved.txt"}, "Setup", false, GitSequence::None)));
    std::filesystem::remove(sandbox.Local() / "gone.txt");
    std::filesystem::rename(sandbox.Local() / "moved.txt", sandbox.Local() / "renamed.txt");
    auto state = sandbox.State();
    const auto folder = std::ranges::find(state.changes, "Art/", &GitChange::path);
    REQUIRE(folder != state.changes.end());
    CHECK(folder->kind == GitChangeKind::Untracked);
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"Art/", "gone.txt", "moved.txt", "renamed.txt"}, "Everything",
                           false, GitSequence::None)));
    state = sandbox.State();
    CHECK(state.changes.empty());
    CHECK(client.Run({"log", "--format=", "--name-only", "-1"}).output.find("hero.png") != std::string::npos);
}

TEST_CASE("Editor.Git.BrandNewProjectCommitsAndPublishes", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    const auto folder = sandbox.Root() / "project";
    Write(folder / "Game" / "main.cpp", "int main() {}\n");
    const auto client = sandbox.At(folder);
    REQUIRE(Fine(GitInitialize(client, {})));
    CHECK(Read(folder / ".gitignore").find("build/") != std::string::npos);
    sandbox.Configure(folder);
    GitRepositoryCache cache;
    auto state = ReadGitRepository(client, cache, true);
    REQUIRE(state.has_value());
    CHECK(state->branch.oid.empty());
    CHECK(NextGitStep(*state) == GitStep::Commit);
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"Game/", ".gitignore", ".gitattributes"}, "First", false,
                           GitSequence::None)));
    state = ReadGitRepository(client, cache, true);
    REQUIRE(state.has_value());
    CHECK(NextGitStep(*state) == GitStep::None); // No remote: nothing to nag about
    std::filesystem::create_directories(sandbox.Remote());
    sandbox.Must(sandbox.Remote(), {"init", "--bare"});
    REQUIRE(Fine(GitAddRemote(client, sandbox.Remote().generic_string())));
    CHECK_FALSE(Fine(GitAddRemote(client, "--upload-pack=evil"))); // Never read as an option
    REQUIRE(Fine(GitPush(client, sandbox.Context(), {"origin"})));
    state = ReadGitRepository(client, cache, true);
    REQUIRE(state.has_value());
    CHECK_FALSE(state->branch.upstream.empty());
    CHECK(NextGitStep(*state) == GitStep::None);
}

TEST_CASE("Editor.Git.CommitAcceptsAMessageWithAnyText", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    Write(sandbox.Local() / "a.txt", "x\n");
    const auto client = sandbox.At(sandbox.Local());
    const std::string message = "Düzelt: \"çift\" tırnak -m --amend";
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"a.txt"}, message, false, GitSequence::None)));
    CHECK(client.Run({"log", "-1", "--format=%s"}).output.starts_with(message));
}

TEST_CASE("Editor.Git.PullFastForwardsAroundUnrelatedChanges", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    sandbox.TeammateCommits("theirs.txt", "new\n");
    Write(sandbox.Local() / "readme.txt", "my edit\n");
    const auto state = sandbox.State();
    CHECK(state.branch.behind == 1);
    REQUIRE(state.incoming.commits.size() == 1);
    CHECK(state.incoming.overlapping.empty());
    REQUIRE(Fine(GitPull(sandbox.At(sandbox.Local()), sandbox.Context())));
    CHECK(std::filesystem::exists(sandbox.Local() / "theirs.txt"));
    CHECK(Read(sandbox.Local() / "readme.txt") == "my edit\n");
}

TEST_CASE("Editor.Git.OverlappingPullRefusesAndStashKeepsTheWork", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    sandbox.TeammateCommits("readme.txt", "one\nteammate line\n");
    Write(sandbox.Local() / "readme.txt", "one\nmy line\n");
    const auto state = sandbox.State();
    CHECK(state.incoming.overlapping == std::vector<std::string>{"readme.txt"});
    const auto client = sandbox.At(sandbox.Local());
    const auto refused = GitPull(client, sandbox.Context());
    CHECK(refused.kind == GitProblemKind::LocalChanges);
    CHECK(Read(sandbox.Local() / "readme.txt") == "one\nmy line\n"); // Nothing was touched
    // Both sides edit the same line: restoring conflicts, and the saved copy stays.
    const auto stashed = GitStashAndPull(client, sandbox.Context(), false);
    CHECK(stashed.kind == GitProblemKind::Conflicts);
    CHECK(client.Run({"stash", "list"}).output.find("before a pull") != std::string::npos);
    const auto after = sandbox.State();
    CHECK(NextGitStep(after) == GitStep::Resolve);
    const auto conflict = std::ranges::find(after.changes, GitChangeKind::Conflicted, &GitChange::kind);
    REQUIRE(conflict != after.changes.end());
    // After a stash, "mine" is the saved version.
    REQUIRE(Fine(GitTakeConflictSide(client, *conflict, GitConflictSide::Mine, after.sequence)));
    CHECK(Read(sandbox.Local() / "readme.txt") == "one\nmy line\n");
}

TEST_CASE("Editor.Git.DivergedBranchRefusesFastForwardThenMerges", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    sandbox.TeammateCommits("theirs.txt", "new\n");
    Write(sandbox.Local() / "mine.txt", "x\n");
    const auto client = sandbox.At(sandbox.Local());
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"mine.txt"}, "Mine", false, GitSequence::None)));
    const auto state = sandbox.State();
    CHECK(state.branch.ahead == 1);
    CHECK(state.branch.behind == 1);
    CHECK(GitPull(client, sandbox.Context()).kind != GitProblemKind::None);
    // Push before pulling is refused too.
    CHECK(GitPush(client, sandbox.Context(), state.remotes).kind == GitProblemKind::PushRejected);
    REQUIRE(Fine(GitMergeIncoming(client, sandbox.Context())));
    CHECK(std::filesystem::exists(sandbox.Local() / "theirs.txt"));
    REQUIRE(Fine(GitPush(client, sandbox.Context(), state.remotes)));
}

TEST_CASE("Editor.Git.ConflictingMergeCanBeResolvedOrAborted", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    sandbox.TeammateCommits("readme.txt", "theirs\n");
    Write(sandbox.Local() / "readme.txt", "mine\n");
    const auto client = sandbox.At(sandbox.Local());
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"readme.txt"}, "Mine", false, GitSequence::None)));
    CHECK(GitMergeIncoming(client, sandbox.Context()).kind == GitProblemKind::Conflicts);
    auto state = sandbox.State();
    CHECK(state.sequence == GitSequence::Merge);
    REQUIRE(Fine(GitAbortSequence(client, state.sequence)));
    CHECK(Read(sandbox.Local() / "readme.txt") == "mine\n");

    CHECK(GitMergeIncoming(client, sandbox.Context()).kind == GitProblemKind::Conflicts);
    state = sandbox.State();
    const auto conflict = std::ranges::find(state.changes, GitChangeKind::Conflicted, &GitChange::kind);
    REQUIRE(conflict != state.changes.end());
    REQUIRE(Fine(GitTakeConflictSide(client, *conflict, GitConflictSide::Theirs, state.sequence)));
    CHECK(Read(sandbox.Local() / "readme.txt") == "theirs\n");
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {}, "", false, state.sequence)));
    CHECK(sandbox.State().sequence == GitSequence::None);
}

TEST_CASE("Editor.Git.LargeFilesAskFirst", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    const auto big = sandbox.Local() / "big.bin";
    {
        std::ofstream out(big, std::ios::binary);
    }
    std::filesystem::resize_file(big, GitLargeFileBytes);
    const auto client = sandbox.At(sandbox.Local());
    const auto refused = GitCommit(client, sandbox.Context(), {"big.bin"}, "Big", false, GitSequence::None);
    CHECK(refused.kind == GitProblemKind::LargeFiles);
    CHECK(refused.files == std::vector<std::string>{"big.bin"});
    // Tracked by LFS, the same file is fine to bring up no warning.
    Write(sandbox.Local() / ".gitattributes", "*.bin filter=lfs diff=lfs merge=lfs -text\n");
    const auto arguments = std::vector<std::string>{"check-attr", "filter", "-z", "--", "big.bin"};
    CHECK(ParseGitLfsPaths(client.Run(arguments).output) == std::vector<std::string>{"big.bin"});
}

TEST_CASE("Editor.Git.DetachedHeadWithLonelyCommitsRefusesToSwitchAway", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    const auto client = sandbox.At(sandbox.Local());
    sandbox.Must(sandbox.Local(), {"switch", "--detach"});
    CHECK(Fine(GitSwitchBranch(client, "main"))); // Nothing to lose: the commit is on main
    sandbox.Must(sandbox.Local(), {"switch", "--detach"});
    Write(sandbox.Local() / "lonely.txt", "x\n");
    sandbox.Must(sandbox.Local(), {"add", "-A"});
    sandbox.Must(sandbox.Local(), {"commit", "-m", "Lonely"});
    CHECK(GitSwitchBranch(client, "main").kind == GitProblemKind::Failed);
    CHECK(std::filesystem::exists(sandbox.Local() / "lonely.txt"));
}

TEST_CASE("Editor.Git.SubmoduleStaysOnItsBranchThroughAPull", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    // A library repository the project records as a submodule, checked out on a branch as a developer works in it.
    const auto libraryRemote = sandbox.Root() / "library.git";
    std::filesystem::create_directories(libraryRemote);
    sandbox.Must(libraryRemote, {"init", "--bare", "-b", "main"});
    sandbox.Must(sandbox.Root(), {"clone", libraryRemote.generic_string(), "library"});
    const auto library = sandbox.Root() / "library";
    sandbox.Configure(library);
    sandbox.Must(library, {"switch", "-c", "main"});
    Write(library / "lib.txt", "v1\n");
    sandbox.Must(library, {"add", "-A"});
    sandbox.Must(library, {"commit", "-m", "V1"});
    sandbox.Must(library, {"push", "-u", "origin", "main"});
    const std::vector<std::string> allowFile{"-c", "protocol.file.allow=always"};
    const auto addSubmodule = [&](const std::filesystem::path& folder) {
        sandbox.Must(folder,
                     {"-c", "protocol.file.allow=always", "submodule", "add", libraryRemote.generic_string(), "lib"});
        sandbox.Must(folder, {"commit", "-m", "Add lib"});
        sandbox.Must(folder, {"push"});
    };
    addSubmodule(sandbox.Local());
    sandbox.Must(sandbox.Teammate(), {"pull"});
    sandbox.Must(sandbox.Teammate(), {"-c", "protocol.file.allow=always", "submodule", "update", "--init"});
    sandbox.Must(sandbox.Local() / "lib", {"switch", "main"});
    sandbox.Configure(sandbox.Local() / "lib");

    // The library moves and a teammate records the new commit.
    Write(library / "lib.txt", "v2\n");
    sandbox.Must(library, {"commit", "-am", "V2"});
    sandbox.Must(library, {"push"});
    const auto teammateLib = sandbox.Teammate() / "lib";
    sandbox.Must(teammateLib, {"fetch"});
    sandbox.Must(teammateLib, {"checkout", "origin/main"});
    sandbox.Must(sandbox.Teammate(), {"add", "lib"});
    sandbox.Must(sandbox.Teammate(), {"commit", "-m", "Move lib"});
    sandbox.Must(sandbox.Teammate(), {"push"});

    const auto state = sandbox.State();
    CHECK(state.incoming.submodules == std::vector<std::string>{"lib"});
    REQUIRE(Fine(GitPull(sandbox.At(sandbox.Local()), sandbox.Context())));
    CHECK(Read(sandbox.Local() / "lib" / "lib.txt") == "v2\n");
    const auto head = sandbox.At(sandbox.Local() / "lib").Run({"symbolic-ref", "--short", "HEAD"}).output;
    CHECK(head.starts_with("main")); // Still on the branch, not detached

    // Local work in the submodule, recorded and unpushed: pushing the project is refused until it is pushed.
    Write(sandbox.Local() / "lib" / "lib.txt", "v3\n");
    sandbox.Must(sandbox.Local() / "lib", {"commit", "-am", "V3"});
    sandbox.Must(sandbox.Local(), {"add", "lib"});
    sandbox.Must(sandbox.Local(), {"commit", "-m", "Record v3"});
    const auto refused = GitPush(sandbox.At(sandbox.Local()), sandbox.Context(), {"origin"});
    CHECK(refused.kind == GitProblemKind::UnpushedSubmodule);
    CHECK(refused.files == std::vector<std::string>{"lib"});
    REQUIRE(Fine(GitPushSubmodules(sandbox.At(sandbox.Local()), sandbox.Context())));
    CHECK(Fine(GitPush(sandbox.At(sandbox.Local()), sandbox.Context(), {"origin"})));

    // A dirty submodule is left alone rather than overwritten.
    Write(sandbox.Local() / "lib" / "lib.txt", "unsaved\n");
    sandbox.Must(teammateLib, {"fetch"});
    sandbox.Must(teammateLib, {"checkout", "origin/main"});
    Write(teammateLib / "lib.txt", "v4\n");
    sandbox.Configure(teammateLib);
    sandbox.Must(teammateLib, {"commit", "-am", "V4"});
    sandbox.Must(teammateLib, {"push", "origin", "HEAD:main"});
    sandbox.Must(sandbox.Teammate(), {"pull"});
    sandbox.Must(sandbox.Teammate(), {"add", "lib"});
    sandbox.Must(sandbox.Teammate(), {"commit", "-m", "Move lib again"});
    sandbox.Must(sandbox.Teammate(), {"push"});
    const auto dirty = GitPull(sandbox.At(sandbox.Local()), sandbox.Context());
    CHECK(dirty.kind == GitProblemKind::SubmoduleNotUpdated);
    CHECK(Read(sandbox.Local() / "lib" / "lib.txt") == "unsaved\n");
}

TEST_CASE("Editor.Git.InitializeKeepsExistingIgnoreRules", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    const auto folder = sandbox.Root() / "project";
    const auto templates = sandbox.Root() / "template";
    Write(templates / "gitignore", "build/\n");
    Write(templates / "gitattributes", "* text=auto\n");
    Write(folder / ".gitignore", "mine/\n");
    const auto client = sandbox.At(folder);
    CHECK_FALSE(FindGitRoot(client).root.has_value());
    REQUIRE(Fine(GitInitialize(client, templates)));
    CHECK(FindGitRoot(client).root.has_value());
    CHECK(Read(folder / ".gitignore") == "mine/\n");
    CHECK(Read(folder / ".gitattributes") == "* text=auto\n");
    REQUIRE(Fine(GitIgnorePath(client, "secret[1].txt")));
    Write(folder / "secret[1].txt", "x\n");
    GitRepositoryCache cache;
    const auto state = ReadGitRepository(client, cache, true);
    REQUIRE(state.has_value());
    CHECK(std::ranges::find(state->changes, "secret[1].txt", &GitChange::path) == state->changes.end());
}

TEST_CASE("Editor.Git.DiffReadsTextAndKeepsBothSidesOfAPicture", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    Write(sandbox.Local() / "art.png", "old pixels");
    const auto client = sandbox.At(sandbox.Local());
    REQUIRE(Fine(GitCommit(client, sandbox.Context(), {"art.png"}, "Art", false, GitSequence::None)));
    Write(sandbox.Local() / "art.png", "new pixels");
    Write(sandbox.Local() / "readme.txt", "two\n");
    const GitChange picture{.path = "art.png", .kind = GitChangeKind::Modified};
    const auto image = ReadGitDiff(client, picture, sandbox.Context());
    CHECK(image.kind == GitDiffKind::Image);
    CHECK(Read(image.before) == "old pixels");
    CHECK(Read(image.after) == "new pixels");
    const GitChange text{.path = "readme.txt", .kind = GitChangeKind::Modified};
    const auto diff = ReadGitDiff(client, text, sandbox.Context());
    CHECK(diff.kind == GitDiffKind::Text);
    CHECK(std::ranges::any_of(diff.lines, [](const GitDiffLine& line) {
        return line.kind == GitDiffLineKind::Added && line.text == "+two";
    }));
    Write(sandbox.Local() / "new.txt", "fresh\n");
    const GitChange untracked{.path = "new.txt", .kind = GitChangeKind::Untracked};
    CHECK(std::ranges::any_of(ReadGitDiff(client, untracked, sandbox.Context()).lines,
                              [](const GitDiffLine& line) { return line.text == "+fresh"; }));
}

TEST_CASE("Editor.Git.ServiceReportsARepositoryAndWaitsForBuilds", "[engine-editor]") {
    Sandbox sandbox;
    if (!sandbox.HasGit()) SKIP("git is not installed");
    sandbox.Start();
    sandbox.TeammateCommits("theirs.txt", "new\n");
    GitService service({sandbox.Local(), sandbox.Root() / "scratch", {}, "MyProject"});
    const auto waitFor = [&](auto condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (!condition(*service.Snapshot()) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return condition(*service.Snapshot());
    };
    // Opening fetches by itself and finds the new commit.
    REQUIRE(waitFor([](const GitSnapshot& snapshot) {
        return snapshot.availability == GitAvailability::Ready && snapshot.Active() &&
               snapshot.Active()->branch.behind == 1;
    }));
    CHECK(service.Snapshot()->Active()->name == "MyProject");
    // A pull waits for the running build, and says so.
    service.SetBuildActive(true);
    service.Request({.kind = GitRequestKind::Pull});
    CHECK(service.SourcesBusy());
    REQUIRE(waitFor([](const GitSnapshot& snapshot) { return snapshot.problem.kind == GitProblemKind::BuildRunning; }));
    CHECK_FALSE(std::filesystem::exists(sandbox.Local() / "theirs.txt"));
    service.SetBuildActive(false);
    const auto changedBefore = service.Snapshot()->sourcesChanged;
    service.Request({.kind = GitRequestKind::Pull});
    REQUIRE(waitFor([&](const GitSnapshot& snapshot) { return snapshot.sourcesChanged == changedBefore + 1; }));
    CHECK(std::filesystem::exists(sandbox.Local() / "theirs.txt"));
    CHECK(service.Snapshot()->notice == "Pulled 1 commit.");
    CHECK_FALSE(service.SourcesBusy());
}
