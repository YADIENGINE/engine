#include "Editor/Internals/Git/GitService.h"
#include <algorithm>
#include <chrono>
#include <format>
#include <utility>
#include "Editor/Internals/Git/GitParsing.h"
#include "Editor/Internals/Platform/Windows/CommandLocator.h"

namespace {
// How long closing the editor waits for an action already running, so it does not stop a pull halfway through writing
// the working tree. A fetch does not wait.
constexpr auto ShutdownGrace = std::chrono::seconds(20);
constexpr auto ShutdownPoll = std::chrono::milliseconds(50);

bool ChangesSources(GitRequestKind kind) {
    switch (kind) {
    case GitRequestKind::Pull:
    case GitRequestKind::StashAndPull:
    case GitRequestKind::Merge:
    case GitRequestKind::AbortSequence:
    case GitRequestKind::SwitchBranch:
    case GitRequestKind::UpdateSubmodules:
    case GitRequestKind::TakeConflictSide: return true;
    default                              : return false;
    }
}
// Network work the developer can reasonably stop.
bool Cancellable(GitRequestKind kind) {
    return kind == GitRequestKind::Fetch || kind == GitRequestKind::Pull || kind == GitRequestKind::Push ||
           kind == GitRequestKind::StashAndPull || kind == GitRequestKind::Merge ||
           kind == GitRequestKind::PushSubmodules;
}
const char* BusyLabel(GitRequestKind kind) {
    switch (kind) {
    case GitRequestKind::Fetch           : return "Fetching";
    case GitRequestKind::Pull            :
    case GitRequestKind::StashAndPull    : return "Pulling";
    case GitRequestKind::Merge           : return "Merging";
    case GitRequestKind::Commit          : return "Committing";
    case GitRequestKind::Push            :
    case GitRequestKind::PushSubmodules  : return "Pushing";
    case GitRequestKind::UpdateSubmodules: return "Updating submodules";
    case GitRequestKind::SwitchBranch    : return "Switching branch";
    default                              : return "Working";
    }
}
// Looking around: no status message to replace, no activity to show, nothing new to read but the working tree.
bool Quiet(GitRequestKind kind) {
    return kind == GitRequestKind::Diff || kind == GitRequestKind::CloseDiff || kind == GitRequestKind::Refresh ||
           kind == GitRequestKind::SelectRepository || kind == GitRequestKind::Dismiss;
}
std::string PluralFiles(std::size_t count) { return std::format("{} file{}", count, count == 1 ? "" : "s"); }
bool Fine(const GitProblem& problem) { return problem.kind == GitProblemKind::None; }
} // namespace

GitService::GitService(GitServiceOptions options)
    : _options(std::move(options))
    , _published(std::make_shared<const GitSnapshot>())
    , _thread([this](const std::stop_token& stop) { Run(stop); }) {}

GitService::~GitService() {
    _thread.request_stop();
    _cancelBackground = true;
    _wake.notify_all();
    const auto deadline = std::chrono::steady_clock::now() + ShutdownGrace;
    while (_performing && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(ShutdownPoll);
    _cancel = true;
}

std::shared_ptr<const GitSnapshot> GitService::Snapshot() const {
    const std::scoped_lock lock(_mutex);
    return _published;
}

void GitService::Request(GitRequest request) {
    if (request.kind == GitRequestKind::Cancel) {
        _cancel = true;
        return;
    }
    if (ChangesSources(request.kind)) ++_sourceActions;
    {
        const std::scoped_lock lock(_mutex);
        _requests.push_back(std::move(request));
        _cancelBackground = true; // A click never waits behind a slow background fetch; the fetch runs again after it
    }
    _wake.notify_all();
}

void GitService::SetFocused(bool focused) {
    {
        const std::scoped_lock lock(_mutex);
        _focusChange = focused;
    }
    _wake.notify_all();
}

std::vector<std::string> GitService::TakeLog() {
    const std::scoped_lock lock(_mutex);
    return std::exchange(_log, {});
}

double GitService::Now() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - _start).count();
}

void GitService::Log(std::string line) {
    const std::scoped_lock lock(_mutex);
    _log.push_back(std::move(line));
}

void GitService::Publish() {
    auto copy = std::make_shared<const GitSnapshot>(_state);
    const std::scoped_lock lock(_mutex);
    _published = std::move(copy);
}

void GitService::SetProblem(GitProblem problem) {
    if (problem.kind != GitProblemKind::None && problem.kind != GitProblemKind::Canceled) {
        auto line = "Git: " + problem.title;
        if (!problem.detail.empty()) line += "\n" + problem.detail;
        Log(std::move(line));
    }
    _state.problem = problem.kind == GitProblemKind::Canceled ? GitProblem{} : std::move(problem);
}

std::optional<GitClient> GitService::ActiveClient() const {
    if (const auto* repository = _state.Active()) return GitClient(_executable, repository->directory);
    return std::nullopt;
}

void GitService::Run(const std::stop_token& stop) {
    std::error_code error;
    std::filesystem::remove_all(_options.scratch, error); // Previews of the last session
    while (!stop.stop_requested()) {
        std::optional<GitRequest> request;
        {
            std::unique_lock lock(_mutex);
            const auto wait = std::chrono::duration<double>(_schedule.WaitSeconds(Now()));
            _wake.wait_for(lock, stop, wait, [&] { return !_requests.empty() || _focusChange.has_value(); });
            if (stop.stop_requested()) break;
            if (_focusChange) {
                const bool focused = *std::exchange(_focusChange, std::nullopt);
                _schedule.SetFocused(focused, Now());
                // Git may have been installed while the window was away.
                if (focused && _state.availability == GitAvailability::Missing) _checkAvailability = true;
            }
            if (!_requests.empty()) {
                request = std::move(_requests.front());
                _requests.pop_front();
                // Only the newest diff matters when the developer clicks through files.
                while (request->kind == GitRequestKind::Diff && !_requests.empty() &&
                       _requests.front().kind == GitRequestKind::Diff) {
                    request = std::move(_requests.front());
                    _requests.pop_front();
                }
            }
        }
        if (request) {
            Handle(*request);
            continue;
        }
        if (_schedule.FetchDue(Now()) && _state.availability == GitAvailability::Ready) {
            Fetch();
            continue;
        }
        if (_schedule.RefreshDue(Now()) || _checkAvailability) Refresh(false);
    }
}

void GitService::Handle(const GitRequest& request) {
    const bool source = ChangesSources(request.kind);
    _performing = true;
    const auto before = _state.Active() ? _state.Active()->branch.oid : std::string{};
    if (source && _buildActive) {
        SetProblem({GitProblemKind::BuildRunning, "Wait for the build to finish, or cancel it, then try again."});
    } else {
        Perform(request);
    }
    _state.busy.clear();
    _state.canCancel = false;
    // Looking at a file or closing it changes nothing to read again.
    const bool lookOnly = request.kind == GitRequestKind::Diff || request.kind == GitRequestKind::CloseDiff ||
                          request.kind == GitRequestKind::Dismiss;
    if (!lookOnly) Refresh(!Quiet(request.kind) || request.kind == GitRequestKind::Refresh);
    if (source && _state.Active() && _state.Active()->branch.oid != before) ++_state.sourcesChanged;
    // In this order: whoever sees the new count must also see that builds may run again.
    if (source) --_sourceActions;
    Publish();
    _performing = false;
}

void GitService::Perform(const GitRequest& request) {
    _cancel = false;
    if (!Quiet(request.kind)) {
        _state.notice.clear();
        _state.problem = {};
        _state.busy = BusyLabel(request.kind);
        _state.canCancel = Cancellable(request.kind);
        Publish();
    }
    if (request.kind == GitRequestKind::Dismiss) {
        _state.problem = {};
        _state.notice.clear();
        return;
    }
    if (request.kind == GitRequestKind::Refresh) return;
    if (request.kind == GitRequestKind::SelectRepository) {
        _state.active = request.index;
        _state.diff.reset();
        return;
    }
    if (request.kind == GitRequestKind::CloseDiff) {
        _state.diff.reset();
        return;
    }
    if (request.kind == GitRequestKind::Initialize) {
        const GitClient client(_executable, _options.directory);
        SetProblem(GitInitialize(client, _options.templateDirectory));
        if (Fine(_state.problem)) _state.notice = "Repository created. Commit your project to save this version.";
        _root.reset();
        _checkAvailability = true;
        return;
    }
    if (request.kind == GitRequestKind::TrustDirectory) {
        const GitClient client(_executable, _options.directory);
        const auto detail = _state.fetchProblem.detail;
        SetProblem(GitTrustDirectory(client, detail));
        _checkAvailability = true;
        return;
    }
    const auto client = ActiveClient();
    const auto* repository = _state.Active();
    if (!client || !repository) return;
    const GitActionContext context{&_cancel, _options.scratch};
    const auto outcome = [&](GitProblem problem, std::string notice) {
        const bool fine = Fine(problem);
        SetProblem(std::move(problem));
        if (fine && !notice.empty()) {
            _state.notice = notice;
            Log("Git: " + std::move(notice));
        }
        return fine;
    };
    const auto findChange = [&](const std::string& path) -> const GitChange* {
        const auto found = std::ranges::find(repository->changes, path, &GitChange::path);
        return found != repository->changes.end() ? &*found : nullptr;
    };
    const auto pulled = [&] {
        const auto behind = repository->branch.behind;
        return std::format("Pulled {} commit{}.", behind, behind == 1 ? "" : "s");
    };
    const auto hasUntrackedOverlap = [&] {
        return std::ranges::any_of(repository->incoming.overlapping, [&](const std::string& file) {
            return std::ranges::any_of(repository->changes, [&](const GitChange& change) {
                return change.kind == GitChangeKind::Untracked &&
                       (change.path == file || (change.path.ends_with('/') && file.starts_with(change.path)));
            });
        });
    };
    switch (request.kind) {
    case GitRequestKind::Fetch:
        _schedule.ResumeFetching();
        _state.fetching = true;
        Publish();
        _state.fetchProblem = GitFetch(*client, GitAccess::Interactive, &_cancel);
        _state.fetching = false;
        if (_state.fetchProblem.kind == GitProblemKind::Canceled) _state.fetchProblem = {};
        if (!Fine(_state.fetchProblem)) Log("Git: " + _state.fetchProblem.title + "\n" + _state.fetchProblem.detail);
        _schedule.FetchFinished(Now());
        break;
    case GitRequestKind::Pull:
        _schedule.ResumeFetching();
        (void)outcome(GitPull(*client, context), pulled());
        break;
    case GitRequestKind::StashAndPull:
        _schedule.ResumeFetching();
        (void)outcome(GitStashAndPull(*client, context, hasUntrackedOverlap()), pulled());
        break;
    case GitRequestKind::Merge:
        _schedule.ResumeFetching();
        (void)outcome(GitMergeIncoming(*client, context), "Merged the remote's commits.");
        break;
    case GitRequestKind::AbortSequence:
        (void)outcome(GitAbortSequence(*client, repository->sequence), "Canceled. Files are as they were before.");
        break;
    case GitRequestKind::UpdateSubmodules:
        (void)outcome(GitSyncSubmodules(*client, context), "Submodules updated.");
        break;
    case GitRequestKind::Commit: {
        const auto count = request.paths.size();
        const bool merge = repository->sequence == GitSequence::Merge;
        const auto notice = merge ? std::string("Merge committed.") : "Committed " + PluralFiles(count) + ".";
        if (const auto build = GitPathInside(_options.scratch.parent_path(), repository->directory);
            !build.empty() && std::ranges::any_of(request.paths, [&](const std::string& path) {
                return path.starts_with(build) || path + "/" == build;
            })) {
            SetProblem({GitProblemKind::Failed,
                        "The build folder is in this selection. Builds do not belong in a repository: right-click it "
                        "and choose Add to .gitignore."});
            break;
        }
        if (outcome(GitCommit(*client, context, request.paths, request.text, request.allowLarge, repository->sequence),
                    notice)) {
            ++_state.committed;
            if (request.thenPush) {
                _state.busy = BusyLabel(GitRequestKind::Push);
                _state.canCancel = true;
                Publish();
                _schedule.ResumeFetching();
                (void)outcome(GitPush(*client, context, repository->remotes), "Committed and pushed.");
            }
        }
        break;
    }
    case GitRequestKind::Push:
        _schedule.ResumeFetching();
        (void)outcome(GitPush(*client, context, repository->remotes), "Pushed.");
        break;
    case GitRequestKind::PushSubmodules:
        _schedule.ResumeFetching();
        if (outcome(GitPushSubmodules(*client, context), {}))
            (void)outcome(GitPush(*client, context, repository->remotes), "Pushed the submodules and this repository.");
        break;
    case GitRequestKind::AddRemote:
        if (outcome(GitAddRemote(*client, request.text), {})) {
            _caches.erase(repository->directory.generic_wstring());
            (void)outcome(GitPush(*client, context, {"origin"}), "Published.");
        }
        break;
    case GitRequestKind::SwitchBranch:
        (void)outcome(GitSwitchBranch(*client, request.text), "Switched to " + request.text + ".");
        break;
    case GitRequestKind::CreateBranch:
        (void)outcome(GitCreateBranch(*client, request.text), "Created " + request.text + ".");
        break;
    case GitRequestKind::SetIdentity:
        (void)outcome(GitSetIdentity(*client, request.text, request.secondText), {});
        break;
    case GitRequestKind::TakeConflictSide:
        if (const auto* change = request.paths.empty() ? nullptr : findChange(request.paths.front()))
            (void)outcome(GitTakeConflictSide(*client, *change, request.side, repository->sequence), {});
        break;
    case GitRequestKind::MarkResolved:
        if (!request.paths.empty()) (void)outcome(GitMarkResolved(*client, request.paths.front()), {});
        break;
    case GitRequestKind::Ignore:
        if (!request.paths.empty()) (void)outcome(GitIgnorePath(*client, request.paths.front()), {});
        break;
    case GitRequestKind::RemoveLock: (void)outcome(GitRemoveLock(*client), "Removed the lock file."); break;
    case GitRequestKind::Diff:
        if (const auto* change = request.paths.empty() ? nullptr : findChange(request.paths.front())) {
            auto loading = std::make_shared<GitDiff>();
            loading->kind = GitDiffKind::Loading;
            loading->path = change->path;
            _state.diff = std::move(loading);
            Publish();
            _state.diff = std::make_shared<const GitDiff>(ReadGitDiff(*client, *change, context));
        }
        break;
    default: break;
    }
}

void GitService::Fetch() {
    {
        const std::scoped_lock lock(_mutex);
        if (!_requests.empty()) return; // The request runs first; the fetch stays due
        _cancelBackground = false;
    }
    if (_thread.get_stop_token().stop_requested()) _cancelBackground = true;
    _state.fetching = true;
    Publish();
    auto repositories = _state.repositories; // Fetching takes long; a request's refresh may replace the list
    GitProblem rootProblem;
    for (std::size_t index = 0; index < repositories.size(); ++index) {
        if (repositories[index].remotes.empty()) continue;
        const GitClient client(_executable, repositories[index].directory);
        auto problem = GitFetch(client, GitAccess::Background, &_cancelBackground);
        if (index == 0) rootProblem = std::move(problem);
    }
    _state.fetching = false;
    if (_cancelBackground) { // A request interrupted it, or the editor is closing: still due
        Publish();
        return;
    }
    // A refused sign-in stays refused until someone acts: retrying could lock the account.
    if (rootProblem.kind == GitProblemKind::Authentication) _schedule.PauseFetching();
    _state.fetchProblem = std::move(rootProblem);
    _schedule.FetchFinished(Now());
    Refresh(true);
    Publish();
}

void GitService::Refresh(bool force) {
    const auto started = std::chrono::steady_clock::now();
    if (_executable.empty() || _checkAvailability) {
        _executable = FindGitExecutable();
        if (_executable.empty()) {
            _state.availability = GitAvailability::Missing;
            _state.repositories.clear();
            _checkAvailability = false;
            _schedule.SetFetchPossible(false);
            _schedule.RefreshFinished(Now(), 0);
            Publish();
            return;
        }
    }
    if (!_root || _checkAvailability) {
        const GitClient client(_executable, _options.directory);
        auto found = FindGitRoot(client);
        _root = std::move(found.root);
        if (!_root) {
            _state.availability = GitAvailability::NoRepository;
            _state.repositories.clear();
            // Why there is none (an untrusted folder) needs a button; the fetch problem outlives actions.
            _state.fetchProblem = std::move(found.problem);
            _checkAvailability = false;
            _schedule.SetFetchPossible(false);
            _schedule.RefreshFinished(Now(), 0);
            Publish();
            return;
        }
        if (_state.fetchProblem.kind == GitProblemKind::DubiousOwnership) _state.fetchProblem = {};
    }
    _checkAvailability = false;
    const GitClient rootClient(_executable, *_root);
    std::vector<GitRepositoryState> repositories;
    const auto read = [&](const GitClient& client, std::string name, bool submodule) {
        auto state = ReadGitRepository(client, _caches[client.Directory().generic_wstring()], force);
        if (!state) return false;
        state->name = std::move(name);
        state->isSubmodule = submodule;
        repositories.push_back(std::move(*state));
        return true;
    };
    if (!read(rootClient, _options.projectName, false)) {
        // The folder may have stopped being a repository; look again next time.
        _root.reset();
        _schedule.SetFetchPossible(false);
        _schedule.RefreshFinished(Now(), 0);
        return;
    }
    for (const auto& path : GitSubmodulePaths(rootClient)) {
        if (!GitSubmoduleInitialized(rootClient, path)) continue;
        const std::filesystem::path relative(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
        (void)read(rootClient.At(*_root / relative), path, true);
    }
    _state.availability = GitAvailability::Ready;
    _state.repositories = std::move(repositories);
    if (_state.active >= _state.repositories.size()) _state.active = 0;
    _schedule.SetFetchPossible(!_state.repositories.front().remotes.empty());
    _schedule.RefreshFinished(Now(), std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
    Publish();
}
