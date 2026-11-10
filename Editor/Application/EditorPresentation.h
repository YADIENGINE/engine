#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Editor/Internals/Agent/AgentReport.h"
#include "Editor/Internals/Git/GitTypes.h"
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Editor/Internals/Session/AudioUsage.h"
#include "Editor/Internals/Session/AudioWaveformCache.h"
#include "Editor/Internals/Session/ProfilerSession.h"
#include "Tuning/TuningFile.h"

class TuningWorkspace;

enum class EditorCommand { Open, Stop, Pause, Reset, Refresh, Cancel, Step, BuildAndPlay, Count };
// What the Inspector asks of a tuning file (Docs/Tuning.md → Editor). Preview sends a dragged value to the running
// scene only; Set writes it to the file, which ends the preview. EndPreview drops a preview without writing. Default
// removes a project setting so the engine's default applies (Docs/ProjectSettings.md); SetKeys writes one player's keys
// for one button of an input map (Docs/InputMap.md).
enum class TuningEditKind { Preview, EndPreview, Set, Resize, Revert, Undo, Redo, Default, SetKeys };
struct TuningEditRequest {
    TuningEditKind kind = TuningEditKind::Set;
    std::filesystem::path source; // Absolute; unused by Undo / Redo
    TuningLocation at;            // Revert, Default: at.field
    TuningValue value;
    TuningResize resize = TuningResize::Append;
    std::size_t player = 0; // SetKeys
    std::string button;
    std::vector<std::string> keys;
};
// The scene's phase. Building only while no scene runs; a running scene stays Ready / Paused while it builds.
enum class EditorPhase { ReadingCatalog, Starting, Ready, Paused, Building, Stopped, Error };
// clang-format off
enum class EditorProfilerCommand { Enable, Disable, Start, Cancel, SaveBaseline, CompareBaseline, ExportJson, ExportMarkdown, GlowFull, GlowReduced, GlowOff };
// clang-format on

struct EditorCapabilities {
    std::array<bool, static_cast<std::size_t>(EditorCommand::Count)> enabled{};
    std::array<const char*, static_cast<std::size_t>(EditorCommand::Count)> disabledReasons{};
    bool playbackEnabled = false;
    const char* playbackDisabledReason = "Run a scene first.";
    [[nodiscard]] bool Can(EditorCommand command) const {
        const auto index = static_cast<std::size_t>(command);
        return index < enabled.size() && enabled[index];
    }
    [[nodiscard]] const char* Reason(EditorCommand command) const {
        const auto index = static_cast<std::size_t>(command);
        return index < disabledReasons.size() && disabledReasons[index] ? disabledReasons[index] : "";
    }
};

// The sound loaded for preview. It plays from its source file in the editor process, apart from the running scene.
struct AudioPreviewState {
    std::filesystem::path file; // Empty until a sound is loaded
    bool playing = false;
    bool paused = false;
    bool loop = false;
    float volume = 1.0f;
    float position = 0.0f; // Seconds
    float length = 0.0f;
    std::string error; // Why the last Play failed
    WaveformEntry waveform;
    std::uint64_t handled = 0; // Requests the host has carried out, counting from 1
};

// What an entry of the agents' activity is: a command (open, action, playback, ...), a changed parameter, a capture or
// an agent's own note.
enum class EditorControlEventKind { Command, Change, Capture, Note };

// Something an agent tool did through the control endpoint.
struct EditorControlEvent {
    std::chrono::system_clock::time_point time;
    std::string text;
    // A changed parameter, undoable while that scene stays open.
    std::string scene;
    std::string parameter;
    std::optional<SceneValue> previous;
    std::filesystem::path file; // A screenshot it took
    EditorControlEventKind kind = EditorControlEventKind::Command;
    // The Console session that sent it (ENGINE_EDITOR_SESSION, see EditorAgentReport); "" from outside the Console.
    std::string session;
};

// The open scene's simulation, for the toolbar.
struct EditorSimulationStatus {
    std::uint64_t step = 0;
    bool replaying = false;
    bool differs = false; // A played replay disagrees with its recorded hashes
};

struct EditorSnapshot {
    const std::vector<SceneDefinition>& catalog;
    const SceneDefinition* selected;
    const SceneParameters& parameters;
    std::set<std::string> pending;
    const EditorDiagnostics& diagnostics;
    EditorCapabilities capabilities;
    EditorPhase phase;
    const std::string& status;
    bool ready = false;
    bool paused = false;
    bool busy = false;
    bool building = false; // Build & Refresh is building or starting the new build beside the running scene
    Vec2 canvas;
    std::uint64_t generation = 0;
    double streamFps = 0.0;
    double captureMs = 0.0;
    double publishMs = 0.0;
    SceneInspection inspection;
    double playbackSpeed = 1.0;
    std::optional<EditorSimulationStatus> simulation;
    ProfilerSessionSnapshot profiler;
    double hostTransferMs = 0.0;
    bool benchmarkCancelPending = false;
    const TuningWorkspace* tuning = nullptr; // The project's tuning files; null without a project
    std::filesystem::path inspectedTuning;   // The tuning file the Inspector shows instead of the scene; empty: none
    AudioPreviewState audio;                 // The Audio Preview panel's sound; set by the host
    const AudioUsage* audioUsage = nullptr;  // Which sound files the project's audio catalog plays
    std::shared_ptr<const PixelatorAsset> pixels; // The Pixelator's pixel graphic; null when none is open
    bool pixelsPending = false;                   // Asked of the worker, not answered yet
    std::string notice;                           // Last change made through the control endpoint
    double noticeAge = 0.0;
    const std::deque<EditorControlEvent>* controlEvents = nullptr; // Oldest first; the latest ones when there were many
    std::size_t controlEventCount = 0;                             // All of this session's, kept or not
    std::uint64_t controlAttempt =
        0; // Build & Refresh attempt started through the control endpoint                  // Seconds since it was made
    const GitSnapshot* git = nullptr; // Source control; null without a project
    // Agents' notes, tool lines and who holds the editor (EditorControlService); null without a control endpoint.
    const EditorAgentReport* agents = nullptr;
    bool pausedForMarking = false; // Confirmed pause requested by Mark, never inferred from a gesture.
};

enum class AudioPreviewAction { Play, Pause, Resume, Stop, Seek, Volume, Loop };
struct AudioPreviewRequest {
    AudioPreviewAction action = AudioPreviewAction::Stop;
    std::filesystem::path file; // Play: the source file, absolute
    bool loop = false;
    float volume = 1.0f;
    float seconds = 0.0f; // Seek
};

// A picture loaded for the source control's before/after view.
struct EditorGitImage {
    std::uint64_t texture = 0; // 0: could not be loaded
    int width = 0;
    int height = 0;
};

// Where one Assets panel file action is: queued or running, finished, or refused with the command's reason.
enum class AssetCommandState { Running, Done, Failed };
struct AssetCommandResult {
    AssetCommandState state = AssetCommandState::Running;
    std::string message; // Failed: the reason, one line
};

// References/callbacks are borrowed for the current UI frame; panels never retain session data.
struct EditorUiActions {
    std::function<void(EditorCommand)> command;
    std::function<void(const std::string&)> select;
    std::function<void(const std::string&, const SceneValue&)> parameter;
    std::function<void(const std::string&)> action;
    std::function<void()> clearOutput;
    std::function<void()> acknowledgeOutput;
    std::function<void(double, bool)> playback;
    std::function<void(EditorProfilerCommand, Profiler::Recipe, const std::string&)> profiler;
    std::function<void(bool)> openOutputLog;
    std::function<void(const TuningEditRequest&)> tuning;
    // Shows a tuning file in the Inspector (empty: back to the selected scene).
    std::function<void(const std::filesystem::path&)> inspectTuning;
    std::function<void(const AudioPreviewRequest&)> previewAudio;
    // Opens a file or folder in its default application.
    std::function<void(const std::filesystem::path&)> openPath;
    // A graphic's thumbnail texture by catalog id; 0 while there is none.
    std::function<std::uint64_t(const std::string&)> assetThumbnail;
    // Arguments after `python Engine asset`: the Assets panel's file actions. Returns the request's id for
    // assetCommandResult.
    std::function<std::uint64_t(const std::vector<std::string>&)> assetCommand;
    std::function<AssetCommandResult(std::uint64_t)> assetCommandResult;
    // A file named in build output, at a line (0: none), in the developer's code editor.
    std::function<void(const std::string&, int)> openSource;
    std::function<void(const GitRequest&)> git;
    std::function<EditorGitImage(const std::filesystem::path&)> gitImage;
    // The Pixelator's pictures: uploads RGBA pixels (rows top to bottom) into texture `slot` (a small number the panel
    // picks) and returns its GL texture, drawn with nearest filtering and repeating; 0 when it could not be made.
    std::function<std::uint64_t(std::size_t slot, int width, int height, std::span<const Color4> pixels)>
        pixelatorTexture;
};
