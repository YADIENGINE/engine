#include "Editor/Application/EditorController.h"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include "Platform/Internals/FileStorage.h"
#include "Platform/Internals/UserData.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

void EditorController::Profile(EditorProfilerCommand command, const Profiler::Recipe& recipe, const std::string& file) {
    try {
        if (command == EditorProfilerCommand::GlowFull || command == EditorProfilerCommand::GlowReduced ||
            command == EditorProfilerCommand::GlowOff) {
            if (_ready && !_profiler.Snapshot().busy)
                Send("profiler",
                     {{"operation", "glow"},
                      {"level", static_cast<int>(command) - static_cast<int>(EditorProfilerCommand::GlowFull)}});
            return;
        }
        if (command == EditorProfilerCommand::Enable || command == EditorProfilerCommand::Disable) {
            if (_ready && !_profiler.Snapshot().busy)
                Send("profiler", {{"operation", "enable"}, {"enabled", command == EditorProfilerCommand::Enable}});
            return;
        }
        if (command == EditorProfilerCommand::Start) {
            // A benchmark measures one worker: none while Build & Refresh may replace it.
            if (!_ready || Busy() || _refresh.Busy() || !Selected() || !Selected()->benchmark ||
                !_pendingParameters.empty())
                return;
            Profiler::ValidateRecipe(recipe);
            _benchmarkCancelPending = false;
            const auto id = ++_profilerRequest;
            _profiler.Arm(id);
            Send("profiler", {{"operation", "start"},
                              {"captureId", id},
                              {"warmup", recipe.warmupFrames},
                              {"frames", recipe.measureFrames},
                              {"pairs", recipe.pairs}});
            return;
        }
        if (command == EditorProfilerCommand::Cancel) {
            if (!_ready || !_profiler.Snapshot().busy || _benchmarkCancelPending) return;
            Send("profiler", {{"operation", "cancel"}, {"captureId", _profiler.Snapshot().captureId}});
            _benchmarkCancelPending = true;
            return;
        }
        if (_profiler.Snapshot().busy) return;
        std::uint64_t hash = 14695981039346656037ULL;
        for (const unsigned char c : ProjectKey()) {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        std::ostringstream key;
        key << std::hex << hash;
        auto directory = UserDataDirectory() / "EngineEditor" / "Profiler" / key.str();
        const std::filesystem::path name = PreviewWide(file.empty() ? "baseline.json" : file);
        if (name != name.filename() || name == "." || name == "..")
            throw std::invalid_argument("Use a report filename, not a path");
        auto destination = directory / name;
        if (command == EditorProfilerCommand::CompareBaseline) {
            _profiler.SetBaseline(Profiler::Load(destination));
        } else {
            const auto& result = _profiler.Snapshot().result;
            if (!result || !result->completed) throw std::runtime_error("Complete a benchmark first");
            std::filesystem::create_directories(directory);
            if (command == EditorProfilerCommand::ExportMarkdown) {
                destination.replace_extension("md");
                (void)WriteBinaryFileAtomic(destination, Profiler::Markdown(*result));
            } else {
                if (command == EditorProfilerCommand::ExportJson) destination.replace_filename("capture.json");
                Profiler::Save(destination, *result);
                if (command == EditorProfilerCommand::SaveBaseline) _profiler.SetBaseline(*result);
            }
        }
        const auto displayed = PreviewUtf8(destination.generic_wstring());
        _profiler.SetFile(displayed);
        _profiler.Error({});
        Log("Profiler report: " + displayed);
    } catch (const std::exception& error) {
        _profiler.Error(error.what());
        Log(error.what(), EditorLogLevel::Warning);
        if (command == EditorProfilerCommand::Start) _profiler.Disconnected();
    }
}
