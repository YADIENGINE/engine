#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include "Editor/Internals/Git/GitClient.h"
#include "Editor/Internals/Git/GitOperations.h"
#include "Editor/Internals/Git/GitSchedule.h"
#include "Editor/Internals/Git/GitTypes.h"

struct GitServiceOptions {
    std::filesystem::path directory;         // The project folder
    std::filesystem::path scratch;           // Temporary files; emptied at start
    std::filesystem::path templateDirectory; // The engine's project template, for new repositories
    std::string projectName;
};

// Runs git on its own thread so the editor never waits for it. Reads and fetches itself on GitSchedule; everything the
// developer asks for goes through Request, one at a time.
class GitService {
public:
    explicit GitService(GitServiceOptions options);
    ~GitService();
    [[nodiscard]] std::shared_ptr<const GitSnapshot> Snapshot() const;
    void Request(GitRequest request);
    void SetFocused(bool focused);
    // Pull and the like wait for a running build: changing sources under it gives a build nobody can trust.
    void SetBuildActive(bool active) { _buildActive = active; }
    // True from the moment such an action is requested: a build must not start under it either.
    [[nodiscard]] bool SourcesBusy() const { return _sourceActions > 0; }
    // Outcomes and git's own words for the Output panel.
    [[nodiscard]] std::vector<std::string> TakeLog();

private:
    void Run(const std::stop_token& stop);
    void Handle(const GitRequest& request);
    void Perform(const GitRequest& request);
    void Fetch();
    void Refresh(bool force);
    void Publish();
    void Log(std::string line);
    void SetProblem(GitProblem problem);
    [[nodiscard]] std::optional<GitClient> ActiveClient() const;
    [[nodiscard]] double Now() const;
    GitServiceOptions _options;
    std::filesystem::path _executable;
    std::optional<std::filesystem::path> _root;
    std::map<std::wstring, GitRepositoryCache> _caches; // By folder
    GitSchedule _schedule;
    GitSnapshot _state; // Service thread only; readers get published copies
    bool _checkAvailability = true;
    const std::chrono::steady_clock::time_point _start = std::chrono::steady_clock::now();
    mutable std::mutex _mutex;
    std::condition_variable_any _wake;
    std::shared_ptr<const GitSnapshot> _published;
    std::deque<GitRequest> _requests;
    std::vector<std::string> _log;
    std::optional<bool> _focusChange;
    std::atomic<bool> _cancel{false};           // The developer's Cancel, or shutdown after a grace period
    std::atomic<bool> _cancelBackground{false}; // Shutdown: a fetch stops at once
    std::atomic<bool> _buildActive{false};
    std::atomic<int> _sourceActions{0};
    std::atomic<bool> _performing{false};
    std::jthread _thread; // Last: stops and joins before anything above goes away

public:
    GitService(const GitService&) = delete;
    GitService& operator=(const GitService&) = delete;
};
