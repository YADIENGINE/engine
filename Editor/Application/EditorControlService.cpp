#include "Editor/Application/EditorControlService.h"
#include <windows.h>
#include <exception>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include "Platform/Internals/FileStorage.h"

namespace {
std::string ReadText(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), {}};
}
} // namespace

EditorControlService::EditorControlService(const std::filesystem::path& buildDirectory)
    : _announcement(buildDirectory / EditorControlFile) {
    std::filesystem::create_directories(buildDirectory);
    (void)WriteBinaryFileAtomic(_announcement, EncodeControlAnnouncement({_pipe.Name(), GetCurrentProcessId()}));
}
EditorControlService::~EditorControlService() {
    try { // Another editor of the same project may have announced itself since; leave its file alone.
        if (DecodeControlAnnouncement(ReadText(_announcement)).pipe != _pipe.Name()) return;
        std::error_code ignored;
        std::filesystem::remove(_announcement, ignored);
    } catch (...) {
    }
}
void EditorControlService::Tick(EditorController& controller) {
    try {
        for (const auto& line : _pipe.Poll())
            Handle(line, controller);
        if (!_watch) return;
        if (!_pipe.Connected() || _pipe.Client() != _watch->client) {
            _watch.reset(); // The client left; the attempt goes on without it
            return;
        }
        Stream(controller.Diagnostics());
    } catch (const std::exception& error) {
        _watch.reset();
        controller.Log(std::string("Control endpoint: ") + error.what(), EditorLogLevel::Warning);
    }
}
void EditorControlService::Handle(std::string_view line, EditorController& controller) {
    try {
        switch (DecodeControlRequest(line)) {
        case EditorControlCommand::Refresh: {
            if (controller.Diagnostics().Attempt().result != EditorBuildResult::Running) {
                const auto capabilities = controller.Snapshot().capabilities;
                if (!capabilities.Can(EditorCommand::Refresh)) {
                    _pipe.Send(EncodeControlError(std::string("Build & Refresh is unavailable: ") +
                                                  capabilities.Reason(EditorCommand::Refresh)));
                    return;
                }
                controller.Execute(EditorCommand::Refresh);
            }
            _watch = Watch{_pipe.Client(), controller.Diagnostics().Attempt().id, 0};
            Stream(controller.Diagnostics());
            return;
        }
        }
    } catch (const std::exception& error) {
        _pipe.Send(EncodeControlError(error.what()));
    }
}
void EditorControlService::Stream(const EditorDiagnostics& diagnostics) {
    const auto& attempt = diagnostics.Attempt();
    if (attempt.id != _watch->attempt) {
        _watch.reset();
        return;
    }
    for (const auto& entry : diagnostics.AttemptEntries()) {
        if (entry.sequence <= _watch->sentSequence) continue;
        _pipe.Send(EncodeControlLog(entry));
        _watch->sentSequence = entry.sequence;
    }
    if (attempt.result == EditorBuildResult::Running || attempt.result == EditorBuildResult::None) return;
    _pipe.Send(EncodeControlResult(attempt.result));
    _watch.reset();
}
