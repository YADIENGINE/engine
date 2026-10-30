#include "Editor/Application/PreviewBuild.h"
#include <stdexcept>

void PreviewBuild::Start(const ProcessCommand& command, const std::filesystem::path& buildDirectory) {
    if (_active) throw std::logic_error("A preview build is already running");
    _lease = std::make_unique<BuildLease>(buildDirectory);
    _process.Start(command);
    _stopped.reset();
    _active = true;
}
void PreviewBuild::Cancel() {
    if (!_active) return;
    _stopped = PreviewBuildOutcome::Canceled;
    _process.Terminate();
}
void PreviewBuild::Abort() {
    if (!_active) return;
    _stopped = PreviewBuildOutcome::Aborted;
    _process.Terminate();
}
std::optional<PreviewBuildExit> PreviewBuild::Finish() {
    if (!_active) return std::nullopt;
    const auto code = _process.ExitCode();
    if (!code) return std::nullopt;
    const auto outcome = _stopped.value_or(*code == 0 ? PreviewBuildOutcome::Succeeded : PreviewBuildOutcome::Failed);
    PreviewBuildExit exit{outcome, *code, _process.DrainOutput()};
    _process.Clear();
    _lease.reset();
    _active = false;
    return exit;
}
