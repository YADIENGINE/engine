#pragma once
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Platform/Windows/BuildLease.h"
#include "Editor/Internals/Platform/Windows/ChildProcess.h"

// Canceled: the user stopped it. Aborted: the editor terminated it after an error; no attempt result follows.
enum class WorkerBuildOutcome { Succeeded, Failed, Canceled, Aborted };

struct WorkerBuildExit {
    WorkerBuildOutcome outcome = WorkerBuildOutcome::Failed;
    unsigned long code = 0;
    std::vector<std::string> output; // What the process wrote after the last DrainOutput
};

// The project's build command for the worker target. It runs while the current scene keeps working: workers run
// from snapshots, so nothing locks the build output. The lease keeps a second editor out of the same build directory.
class WorkerBuild {
public:
    void Start(const ProcessCommand& command, const std::filesystem::path& buildDirectory);
    [[nodiscard]] bool Active() const { return _active; }
    [[nodiscard]] std::vector<std::string> DrainOutput() { return _process.DrainOutput(); }
    void Cancel();
    void Abort();
    // The exit, reported once after the process has ended; the build is inactive afterwards.
    [[nodiscard]] std::optional<WorkerBuildExit> Finish();

private:
    ChildProcess _process;
    std::unique_ptr<BuildLease> _lease;
    std::optional<WorkerBuildOutcome> _stopped;
    bool _active = false;
};
