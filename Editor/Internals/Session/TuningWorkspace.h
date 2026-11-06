#pragma once
#include <chrono>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Internals/Protocol/WorkerProtocol.h"
#include "Input/InputMap.h"
#include "Tuning/TuningFile.h"

struct TuningFileProblem {
    int line = 0;
    std::string text;
};

// Tuning: X.tuning.json (Docs/Tuning.md). Settings: the project's Game/Game.settings.json against the engine's schema
// (Docs/ProjectSettings.md). InputMap: X.input.json (Docs/InputMap.md).
enum class TuningFileKind { Tuning, Settings, InputMap };

struct TuningFileState {
    TuningFileKind kind = TuningFileKind::Tuning;
    std::filesystem::path source; // Absolute
    std::string label;            // From the folder all tuning files share: "Effects/Glow.tuning.json"
    std::string relative;         // From the project folder, '/'-separated: "Game/Effects/Glow.tuning.json"
    std::string target;           // The target that built it at the last configure; empty: not built yet
    std::vector<TuningField> fields;
    std::optional<TuningFileProblem> problem; // Unreadable or invalid; `fields` keep the last valid values
    std::optional<WorkerTuningStatus> worker; // The running scene's last report; none while it does not use the file
    bool used = false;                        // The running scene read the file since it opened
    // The fields at git HEAD, which Modified and Revert compare with. Empty: unknown yet, not committed, or no git.
    std::optional<std::vector<TuningField>> committed;
    std::optional<std::filesystem::file_time_type> written;
    // Settings: whether the file exists (the first edit writes it), and the names it sets, now and at git HEAD.
    bool exists = true;
    std::vector<std::string> set;
    std::optional<std::vector<std::string>> committedSet;
    // InputMap: the last valid map, and the map at git HEAD.
    std::optional<InputMapDocument> input;
    std::optional<InputMapDocument> committedInput;

    [[nodiscard]] const TuningField* Field(std::string_view name) const;
    [[nodiscard]] const TuningField* CommittedField(std::string_view name) const;
};

// One undone or redone edit: its file (the running scene rereads it) and a line for Output.
struct TuningHistoryStep {
    std::filesystem::path source;
    std::string text;
    bool done = false; // False: the file changed since, so nothing was written
};

// The project's tuning files in the editor (Docs/Tuning.md → Editor). Roots and built files come from the build's
// Tuning/TuningFiles.json (CMake engine_add_tuning); the roots are scanned, so a new file shows before it is built.
// The file is the only place values are kept: every edit reads the file from disk, patches it and writes it
// atomically, and can be undone while the file still holds what the edit wrote.
class TuningWorkspace {
public:
    void Open(const std::filesystem::path& buildDirectory);
    // Rereads changed files at most every 250 ms, and looks for new or removed files every 2 s.
    void Poll();
    [[nodiscard]] const std::vector<TuningFileState>& Files() const { return _files; }
    [[nodiscard]] const TuningFileState* Find(const std::filesystem::path& source) const;
    // The project's settings file; null before the project is configured.
    [[nodiscard]] const TuningFileState* Settings() const;

    // Edits. Empty text: written (or nothing to change); otherwise why the file was left alone (invalid on disk, out
    // of bounds, wrong type).
    [[nodiscard]] std::string Set(const std::filesystem::path& source, const TuningLocation& at,
                                  const TuningValue& value);
    [[nodiscard]] std::string Resize(const std::filesystem::path& source, const TuningLocation& at,
                                     TuningResize resize);
    // Writes a field's committed values back; its shape must be the committed one.
    [[nodiscard]] std::string Revert(const std::filesystem::path& source, const std::string& field);
    // Settings: removes the value from the file, so the engine's default applies.
    [[nodiscard]] std::string ResetToDefault(const std::filesystem::path& source, const std::string& field);
    // InputMap: one player's keys for one button; an empty list leaves the button without keys.
    [[nodiscard]] std::string SetKeys(const std::filesystem::path& source, std::size_t player, const std::string& button,
                                      const std::vector<std::string>& keys);
    [[nodiscard]] bool CanUndo() const { return !_undo.empty(); }
    [[nodiscard]] bool CanRedo() const { return !_redo.empty(); }
    [[nodiscard]] std::optional<TuningHistoryStep> Undo();
    [[nodiscard]] std::optional<TuningHistoryStep> Redo();

    // The running scene's report for a file; returns the file's label, empty when it is not one of these.
    std::string Report(const WorkerTuningStatus& status);
    // The files the running scene has read since it opened (absolute paths).
    void SetUsed(const std::vector<std::filesystem::path>& sources);
    void ClearWorkerStatus();
    // The text at git HEAD; nullopt when the file has none.
    void SetCommitted(const std::filesystem::path& source, const std::optional<std::string>& text);

private:
    struct Edit {
        std::filesystem::path source;
        std::string what; // "Glow.tuning.json: radius"
        std::string before;
        std::string after;
    };
    void ReadList();
    void Scan();
    TuningFileState* FindMutable(const std::filesystem::path& source);
    [[nodiscard]] std::string Write(TuningFileState& file, const std::string& what, const std::string& before,
                                    const std::string& after);
    std::optional<TuningHistoryStep> Step(std::deque<Edit>& from, std::deque<Edit>& to, bool undo);

    std::filesystem::path _list;
    std::filesystem::path _project;
    std::vector<std::pair<std::string, std::filesystem::path>> _roots; // Target, folder
    std::vector<std::pair<std::string, std::filesystem::path>> _built; // Target, file
    std::optional<std::filesystem::file_time_type> _listWritten;
    std::vector<TuningFileState> _files;
    std::deque<Edit> _undo;
    std::deque<Edit> _redo;
    std::chrono::steady_clock::time_point _polledAt{};
    std::chrono::steady_clock::time_point _scannedAt{};
};
