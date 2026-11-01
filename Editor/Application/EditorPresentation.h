#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Editor/Internals/Session/AudioUsage.h"
#include "Editor/Internals/Session/AudioWaveformCache.h"
#include "Editor/Internals/Session/ProfilerSession.h"
#include "Tuning/TuningFile.h"

class TuningWorkspace;

enum class EditorCommand { Open, Stop, Pause, Reset, Refresh, Cancel, Step, Count };
// The scene's phase. Building only while no scene runs; a running scene stays Ready / Paused while it builds.
enum class EditorPhase { ReadingCatalog, Starting, Ready, Paused, Building, Stopped, Error };
// clang-format off
enum class EditorProfilerCommand { Enable, Disable, Start, Cancel, SaveBaseline, CompareBaseline, ExportJson, ExportMarkdown, GlowFull, GlowReduced, GlowOff };
// clang-format on

struct EditorCapabilities {
    std::array<bool, static_cast<std::size_t>(EditorCommand::Count)> enabled{};
    std::array<const char*, static_cast<std::size_t>(EditorCommand::Count)> disabledReasons{};
    bool playbackEnabled = false;
    const char* playbackDisabledReason = "Open a scene first.";
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
    ProfilerSessionSnapshot profiler;
    double hostTransferMs = 0.0;
    bool benchmarkCancelPending = false;
    const TuningWorkspace* tuning = nullptr; // Projenin tuning dosyaları; proje yoksa null
    AudioPreviewState audio;                 // The Audio Preview panel's sound; set by the host
    const AudioUsage* audioUsage = nullptr;  // Which sound files the project's audio catalog plays
    std::string notice;                      // Last change made through the control endpoint
    double noticeAge = 0.0;
    std::uint64_t controlAttempt =
        0; // Build & Refresh attempt started through the control endpoint                  // Seconds since it was made
};

enum class AudioPreviewAction { Play, Pause, Resume, Stop, Seek, Volume, Loop };
struct AudioPreviewRequest {
    AudioPreviewAction action = AudioPreviewAction::Stop;
    std::filesystem::path file; // Play: the source file, absolute
    bool loop = false;
    float volume = 1.0f;
    float seconds = 0.0f; // Seek
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
    // Tuning dosyasındaki bir değeri diske yamar (Enter); dosya sırası TuningWorkspace::Files'tır.
    std::function<void(std::size_t, const TuningLocation&, const TuningValue&)> tune;
    std::function<void(const AudioPreviewRequest&)> previewAudio;
    // Opens a file or folder in its default application.
    std::function<void(const std::filesystem::path&)> openPath;
    // A graphic's thumbnail texture by catalog id; 0 while there is none.
    std::function<std::uint64_t(const std::string&)> assetThumbnail;
    // Arguments after `python Engine asset`: the Assets panel's file actions.
    std::function<void(const std::vector<std::string>&)> assetCommand;
};
