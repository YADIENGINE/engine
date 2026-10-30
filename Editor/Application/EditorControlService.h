#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include "Editor/Application/EditorController.h"
#include "Editor/Internals/Platform/Windows/EditorControlPipe.h"

// Serves the editor's control endpoint (EditorControlProtocol.h) for one session: announces the pipe in the build
// directory and answers requests. `refresh` runs Build & Refresh, or joins the one already running, and streams that
// attempt's log lines and its result to the client.
class EditorControlService {
public:
    explicit EditorControlService(const std::filesystem::path& buildDirectory);
    ~EditorControlService();
    void Tick(EditorController& controller);

private:
    struct Watch {
        std::uint64_t client = 0;
        std::uint64_t attempt = 0;
        std::uint64_t sentSequence = 0;
    };
    void Handle(std::string_view line, EditorController& controller);
    void Stream(const EditorDiagnostics& diagnostics);
    EditorControlPipe _pipe;
    std::filesystem::path _announcement;
    std::optional<Watch> _watch;

public:
    EditorControlService(const EditorControlService&) = delete;
    EditorControlService& operator=(const EditorControlService&) = delete;
};
