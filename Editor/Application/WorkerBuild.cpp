#include "Editor/Application/WorkerBuild.h"
#include <stdexcept>

void WorkerBuild::Start(const ProcessCommand& command, const std::filesystem::path& buildDirectory) {
    if (_active) throw std::logic_error("A scene build is already running");
    _lease = std::make_unique<BuildLease>(buildDirectory);
    _process.Start(command);
    _stopped.reset();
    _active = true;
}
void WorkerBuild::Cancel() {
    if (!_active) return;
    _stopped = WorkerBuildOutcome::Canceled;
    _process.Terminate();
}
void WorkerBuild::Abort() {
    if (!_active) return;
    _stopped = WorkerBuildOutcome::Aborted;
    _process.Terminate();
}
std::optional<WorkerBuildExit> WorkerBuild::Finish() {
    if (!_active) return std::nullopt;
    const auto code = _process.ExitCode();
    if (!code) return std::nullopt;
    const auto outcome = _stopped.value_or(*code == 0 ? WorkerBuildOutcome::Succeeded : WorkerBuildOutcome::Failed);
    WorkerBuildExit exit{outcome, *code, _process.DrainOutput()};
    _process.Clear();
    _lease.reset();
    _active = false;
    return exit;
}
