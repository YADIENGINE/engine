#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"

// The editor's control endpoint: tools reach a running editor through a local named pipe (`python Engine refresh`;
// later the launcher and the agent API). While it runs, the editor announces the pipe in EditorControlFile inside its
// build directory. Messages are JSON objects, one per '\n'-terminated UTF-8 line; a client sends one request and
// reads replies until a "result" or "error". Reference: Docs/Editor/Architecture.md → Control endpoint.
inline constexpr int EditorControlVersion = 1;
inline constexpr std::string_view EditorControlFile = "EngineEditor.control.json";
inline constexpr std::size_t EditorControlMaxLineBytes = 64 * 1024;

struct EditorControlAnnouncement {
    std::string pipe;
    unsigned long processId = 0;
};

enum class EditorControlCommand { Refresh };

[[nodiscard]] std::string EncodeControlAnnouncement(const EditorControlAnnouncement& announcement);
[[nodiscard]] EditorControlAnnouncement DecodeControlAnnouncement(std::string_view json);
// Throws for malformed JSON, another protocol version or an unknown command.
[[nodiscard]] EditorControlCommand DecodeControlRequest(std::string_view line);
[[nodiscard]] std::string EncodeControlLog(const EditorLogEntry& entry);
// Only for a finished attempt (Succeeded, BuildFailed, RefreshFailed, Canceled).
[[nodiscard]] std::string EncodeControlResult(EditorBuildResult result);
[[nodiscard]] std::string EncodeControlError(std::string_view text);
