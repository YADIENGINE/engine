#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <string_view>

// Process-scoped installation, owned by the entry point; never owns game objects or callbacks.
class CrashSession {
public:
    explicit CrashSession(std::string_view kind, bool playerDialog = false) noexcept;
    ~CrashSession();
    CrashSession(const CrashSession&) = delete;
    CrashSession& operator=(const CrashSession&) = delete;
};

// Called only at a fatal exception boundary instrumented with cpptrace::try_catch.
void ReportCrashException(const char* message) noexcept;
[[nodiscard]] int RunCrashBoundary(const std::function<int()>& action) noexcept;
// The cleanup must handle its own secondary failures. Callbacks run immediately and are never stored.
void RunWithCrashCleanup(const std::function<void()>& action, const std::function<void()>& cleanup);
void SetCrashContext(std::string_view context) noexcept;
// Removes incident directories under `root` left by processes that ended without a crash (killed, for example); a
// running session is never touched. With `keep` > 0 only the newest `keep` crash records stay. Never throws.
void PruneCrashReports(const std::filesystem::path& root, std::size_t keep) noexcept;
// The editor knows this worker's session before launch. Includes the final stderr drain before process exit.
[[nodiscard]] bool WorkerCrashPending(std::string_view session) noexcept;
