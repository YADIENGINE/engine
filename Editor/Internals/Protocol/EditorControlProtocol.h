#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <string>
#include <string_view>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"

// The editor's control endpoint: tools reach a running editor through a local named pipe (`python Engine refresh`,
// `python Engine ctl ...`). While it runs, the editor announces the pipe in EditorControlFile inside its build
// directory. Messages are JSON objects, one per '\n'-terminated UTF-8 line; a client sends one request and reads
// replies until a "result" or "error". Reference: Docs/Editor/Automation.md.
inline constexpr int EditorControlVersion = 3;
inline constexpr std::string_view EditorControlFile = "EngineEditor.control.json";
inline constexpr std::size_t EditorControlMaxLineBytes = 64 * 1024;

struct EditorControlAnnouncement {
    std::string pipe;
    unsigned long processId = 0;
};

// Refresh streams build output; the others answer once (Docs/Editor/Automation.md → Commands).
enum class EditorControlCommand {
    Refresh,
    Status,
    List,
    Open,
    Set,
    Action,
    Pause,
    Play,
    Step,
    Speed,
    Reset,
    Seek,
    Screenshot,
    Frames,
    Inspect,
    Logs,
    Note,
    Audio,        // The Audio Preview panel's state
    AudioControl, // Plays, pauses, stops, seeks or changes the Audio Preview sound ("action")
    Count
};

struct EditorControlRequest {
    EditorControlCommand command = EditorControlCommand::Status;
    nlohmann::json arguments = nlohmann::json::object(); // The request without "version" and "command"
};

// What an agent reports with `ctl note` (shown in the status bar): working on a request, waiting for the developer,
// or done.
enum class AgentActivityState { Idle, Working, Waiting };
struct AgentNote {
    AgentActivityState state = AgentActivityState::Working;
    std::string text;
    std::string session; // The Console session it came from (its ENGINE_EDITOR_SESSION); empty from elsewhere
};
inline constexpr std::size_t AgentNoteMaxBytes = 200;
inline constexpr std::size_t AgentSessionMaxBytes = 64;
// {"text": "...", "state": "working" | "waiting" | "idle", "session": "..."} (state optional, default working; session
// optional); text and session are cut to their limits. Throws for an unknown state.
[[nodiscard]] AgentNote DecodeAgentNote(const nlohmann::json& arguments);
[[nodiscard]] const char* AgentActivityName(AgentActivityState state);
// Commands that change the editor (scene, playback, build). The developer's "Agent control" switch refuses them;
// observing commands and notes still answer.
[[nodiscard]] bool ControlCommandChangesEditor(EditorControlCommand command);

[[nodiscard]] std::string EncodeControlAnnouncement(const EditorControlAnnouncement& announcement);
[[nodiscard]] EditorControlAnnouncement DecodeControlAnnouncement(std::string_view json);
// Throws for malformed JSON, another protocol version or an unknown command.
[[nodiscard]] EditorControlRequest DecodeControlRequest(std::string_view line);
[[nodiscard]] std::string_view ControlCommandName(EditorControlCommand command);
[[nodiscard]] std::string EncodeControlLog(const EditorLogEntry& entry);
// Only for a finished attempt (Succeeded, BuildFailed, RefreshFailed, Canceled).
[[nodiscard]] std::string EncodeControlResult(EditorBuildResult result);
// A command's answer: `data` (an object) with "type": "result".
[[nodiscard]] std::string EncodeControlReply(nlohmann::json data);
[[nodiscard]] std::string EncodeControlError(std::string_view text);

// Plain JSON for tools: numbers, booleans, strings; colors as [r, g, b, a].
[[nodiscard]] nlohmann::json ControlValue(const SceneValue& value);
// The value a tool wrote for `parameter`, by its kind; integers accept whole numbers, colors [r, g, b(, a)] or
// "#rrggbb(aa)". Throws with a readable message when it does not fit (range and choices are checked separately).
[[nodiscard]] SceneValue ControlValueOf(const SceneParameter& parameter, const nlohmann::json& value);
// One catalog entry with its parameters (kind, range, choices, current value) and actions.
[[nodiscard]] nlohmann::json ControlCatalogEntry(const SceneDefinition& definition, const SceneParameters& values);
[[nodiscard]] nlohmann::json ControlLogEntry(const EditorLogEntry& entry);
