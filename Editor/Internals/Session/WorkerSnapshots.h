#pragma once
#include <filesystem>
#include <optional>
#include "Editor/Internals/Process/ProcessCommand.h"

// Workers never run from the build output: every successful build is copied to an immutable snapshot, so the next
// build can overwrite the output while a worker still runs from the previous one. A snapshot sits beside the built
// executable (`<name>.live-<n>.exe`), so assets and the working directory resolve exactly as for the build output.
// Debug symbols stay with the build output and match the latest snapshot.
class WorkerSnapshots {
public:
    explicit WorkerSnapshots(ProcessCommand built);
    [[nodiscard]] bool BuildAvailable() const;
    // Copies the current build output into a new snapshot and makes it current.
    void Capture();
    // The command that starts a worker from the current snapshot; empty before the first Capture.
    [[nodiscard]] const std::optional<ProcessCommand>& Current() const { return _current; }
    // Deletes every snapshot but the current one; a snapshot a process still runs from stays until a later Prune.
    void Prune() const;

private:
    [[nodiscard]] bool IsSnapshot(const std::filesystem::path& file) const;
    ProcessCommand _built;
    std::optional<ProcessCommand> _current;
};
