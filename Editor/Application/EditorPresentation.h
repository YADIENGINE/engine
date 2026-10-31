#pragma once
#include <array>
#include <functional>
#include <set>
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Editor/Internals/Session/ProfilerSession.h"
#include "Tuning/TuningFile.h"

class TuningWorkspace;

enum class EditorCommand { Open, Stop, Pause, Reset, Refresh, Cancel, Step, Count };
// The preview's phase. Building only while no preview runs; a running preview stays Ready / Paused while it builds.
enum class EditorPhase { ReadingCatalog, Starting, Ready, Paused, Building, Stopped, Error };
// clang-format off
enum class EditorProfilerCommand { Enable, Disable, Start, Cancel, SaveBaseline, CompareBaseline, ExportJson, ExportMarkdown, GlowFull, GlowReduced, GlowOff };
// clang-format on

struct EditorCapabilities {
    std::array<bool, static_cast<std::size_t>(EditorCommand::Count)> enabled{};
    std::array<const char*, static_cast<std::size_t>(EditorCommand::Count)> disabledReasons{};
    bool playbackEnabled = false;
    const char* playbackDisabledReason = "Open a preview first.";
    [[nodiscard]] bool Can(EditorCommand command) const {
        const auto index = static_cast<std::size_t>(command);
        return index < enabled.size() && enabled[index];
    }
    [[nodiscard]] const char* Reason(EditorCommand command) const {
        const auto index = static_cast<std::size_t>(command);
        return index < disabledReasons.size() && disabledReasons[index] ? disabledReasons[index] : "";
    }
};

struct EditorSnapshot {
    const std::vector<PreviewDefinition>& catalog;
    const PreviewDefinition* selected;
    const PreviewParameters& parameters;
    std::set<std::string> pending;
    const EditorDiagnostics& diagnostics;
    EditorCapabilities capabilities;
    EditorPhase phase;
    const std::string& status;
    bool ready = false;
    bool paused = false;
    bool busy = false;
    bool building = false; // Build & Refresh is building or starting the new build beside the running preview
    Vec2 canvas;
    std::uint64_t generation = 0;
    double streamFps = 0.0;
    double captureMs = 0.0;
    double publishMs = 0.0;
    PreviewInspection inspection;
    double playbackSpeed = 1.0;
    ProfilerSessionSnapshot profiler;
    double hostTransferMs = 0.0;
    bool benchmarkCancelPending = false;
    const TuningWorkspace* tuning = nullptr; // Projenin tuning dosyaları; proje yoksa null
};

// References/callbacks are borrowed for the current UI frame; panels never retain session data.
struct EditorUiActions {
    std::function<void(EditorCommand)> command;
    std::function<void(const std::string&)> select;
    std::function<void(const std::string&, const PreviewValue&)> parameter;
    std::function<void(const std::string&)> action;
    std::function<void()> clearOutput;
    std::function<void()> acknowledgeOutput;
    std::function<void(double, bool)> playback;
    std::function<void(EditorProfilerCommand, Profiler::Recipe, const std::string&)> profiler;
    std::function<void(bool)> openOutputLog;
    // Tuning dosyasındaki bir değeri diske yamar (Enter); dosya sırası TuningWorkspace::Files'tır.
    std::function<void(std::size_t, const TuningLocation&, const TuningValue&)> tune;
};
