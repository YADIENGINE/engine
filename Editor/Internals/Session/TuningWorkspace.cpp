#include "Editor/Internals/Session/TuningWorkspace.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cwctype>
#include "Platform/Internals/FileStorage.h"
#include "Runtime/ProjectSettingsStore.h"

namespace {
constexpr auto PollInterval = std::chrono::milliseconds(250);
constexpr auto ScanInterval = std::chrono::seconds(2);
constexpr std::size_t MaxFileBytes = 4 * 1024 * 1024;
constexpr std::size_t MaxHistory = 200;
constexpr std::string_view Suffix = ".tuning.json";
constexpr std::string_view InputSuffix = ".input.json";
constexpr std::string_view SettingsFile = "Game/Game.settings.json"; // From the project folder
constexpr std::string_view EmptySettings = "{}\n";

std::optional<std::filesystem::file_time_type> WriteTime(const std::filesystem::path& file) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(file, error);
    return error ? std::nullopt : std::optional(time);
}

// The worker, CMake and a folder scan may spell one file differently (separators, letter case); compare by this key.
std::wstring Key(const std::filesystem::path& file) {
    std::wstring key = std::filesystem::absolute(file).lexically_normal().generic_wstring();
    std::transform(key.begin(), key.end(), key.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return key;
}

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}

std::filesystem::path PathOf(const std::string& utf8) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()));
}

// A settings file that does not exist yet reads as one that sets nothing.
std::string Text(const TuningFileState& file) {
    std::error_code error;
    if (file.kind == TuningFileKind::Settings && !std::filesystem::exists(file.source, error))
        return std::string(EmptySettings);
    return ReadBinaryFile(file.source, MaxFileBytes);
}

void Read(TuningFileState& file) {
    file.written = WriteTime(file.source);
    try {
        std::error_code error;
        file.exists = std::filesystem::exists(file.source, error);
        const auto text = Text(file);
        if (file.kind == TuningFileKind::Settings) {
            auto values = ParseSettingsValues(text, ProjectSettingsSchema());
            file.fields = std::move(values.fields);
            file.set = std::move(values.set);
        } else if (file.kind == TuningFileKind::InputMap) {
            file.input = ParseInputMap(text);
        } else {
            file.fields = ParseTuningFile(text);
        }
        file.problem.reset();
    } catch (const TuningFormatError& error) {
        file.problem = TuningFileProblem{error.Line(), error.what()};
    } catch (const std::exception& error) {
        file.problem = TuningFileProblem{0, error.what()};
    }
}

// From the folder the tuning files and input maps share; the settings file has its own label.
void Label(std::vector<TuningFileState>& files) {
    const auto first = std::ranges::find_if(files, [](const auto& f) { return f.kind != TuningFileKind::Settings; });
    if (first == files.end()) return;
    std::filesystem::path common = first->source.parent_path();
    for (const auto& file : files)
        while (file.kind != TuningFileKind::Settings && !common.empty() &&
               Key(file.source).rfind(Key(common) + L"/", 0) != 0) {
            const auto parent = common.parent_path();
            if (parent == common) break;
            common = parent;
        }
    for (auto& file : files)
        file.label = Utf8(file.source.lexically_relative(common));
}

bool IsTuningFile(const std::filesystem::path& file) {
    const auto name = Utf8(file.filename());
    return (name.size() > Suffix.size() && name.ends_with(Suffix)) ||
           (name.size() > InputSuffix.size() && name.ends_with(InputSuffix));
}

TuningFileKind KindOf(const std::filesystem::path& file) {
    return Utf8(file.filename()).ends_with(InputSuffix) ? TuningFileKind::InputMap : TuningFileKind::Tuning;
}

void ScanFolder(const std::filesystem::path& folder, std::vector<std::filesystem::path>& found) {
    std::error_code error;
    auto it = std::filesystem::recursive_directory_iterator(folder, error);
    for (; !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error)) {
        const auto name = Utf8(it->path().filename());
        if (it->is_directory(error)) {
            if (name.starts_with('.') || std::filesystem::exists(it->path() / "CMakeCache.txt", error))
                it.disable_recursion_pending(); // Hidden folders and build trees
            continue;
        }
        if (IsTuningFile(it->path())) found.push_back(it->path().lexically_normal());
    }
}

const TuningField* Named(const std::vector<TuningField>& fields, std::string_view name) {
    const auto found = std::ranges::find(fields, name, &TuningField::name);
    return found == fields.end() ? nullptr : &*found;
}

std::optional<TuningValue> ValueAt(const TuningField& field, const TuningLocation& at) {
    const std::vector<TuningValue>* values = &field.values;
    if (field.table) {
        const auto column = std::ranges::find(field.columns, at.column, &TuningColumn::name);
        if (column == field.columns.end() || at.row >= field.rows.size()) return std::nullopt;
        values = &field.rows[at.row][static_cast<std::size_t>(column - field.columns.begin())];
    }
    if (at.element >= values->size()) return std::nullopt;
    return (*values)[at.element];
}

// Same kind, list lengths, rows and columns: values can be copied over one by one.
bool SameShape(const TuningField& a, const TuningField& b) {
    if (a.table != b.table) return false;
    if (!a.table)
        return a.spec.type == b.spec.type && a.spec.choices == b.spec.choices && a.values.size() == b.values.size();
    if (a.rows.size() != b.rows.size() || a.columns.size() != b.columns.size()) return false;
    for (std::size_t c = 0; c < a.columns.size(); ++c)
        if (a.columns[c].name != b.columns[c].name || a.columns[c].spec.type != b.columns[c].spec.type ||
            a.columns[c].spec.choices != b.columns[c].spec.choices)
            return false;
    for (std::size_t r = 0; r < a.rows.size(); ++r)
        for (std::size_t c = 0; c < a.columns.size(); ++c)
            if (a.rows[r][c].size() != b.rows[r][c].size()) return false;
    return true;
}

std::string Where(const TuningFileState& file, const TuningLocation& at) {
    std::string text = file.label + ": " + at.field;
    if (!at.column.empty()) text += "[" + std::to_string(at.row) + "]." + at.column;
    return text;
}
} // namespace

const TuningField* TuningFileState::Field(std::string_view name) const { return Named(fields, name); }
const TuningField* TuningFileState::CommittedField(std::string_view name) const {
    return committed ? Named(*committed, name) : nullptr;
}

void TuningWorkspace::Open(const std::filesystem::path& buildDirectory) {
    _list = buildDirectory / "Tuning" / "TuningFiles.json";
    _listWritten.reset();
    _files.clear();
    _undo.clear();
    _redo.clear();
    _polledAt = {};
    _scannedAt = {};
    Poll();
}

void TuningWorkspace::Poll() {
    if (_list.empty()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - _polledAt < PollInterval) return;
    _polledAt = now;
    if (WriteTime(_list) != _listWritten) {
        ReadList();
        _scannedAt = {};
    }
    if (now - _scannedAt >= ScanInterval) {
        _scannedAt = now;
        Scan();
    }
    for (auto& file : _files)
        if (WriteTime(file.source) != file.written) Read(file);
}

void TuningWorkspace::ReadList() {
    _listWritten = WriteTime(_list);
    _roots.clear();
    _built.clear();
    _project.clear();
    try {
        const auto json = nlohmann::json::parse(ReadBinaryFile(_list, MaxFileBytes));
        if (const auto project = json.find("project"); project != json.end())
            _project = PathOf(project->get<std::string>());
        for (const auto& entry : json.value("roots", nlohmann::json::array()))
            _roots.emplace_back(entry.at("target").get<std::string>(), PathOf(entry.at("root").get<std::string>()));
        for (const auto& entry : json.at("files"))
            _built.emplace_back(entry.at("target").get<std::string>(), PathOf(entry.at("source").get<std::string>()));
    } catch (const std::exception&) {
        _roots.clear(); // No list or a broken one: the project uses no tuning yet, or is not configured
        _built.clear();
    }
}

void TuningWorkspace::Scan() {
    std::vector<std::filesystem::path> found;
    for (const auto& [target, root] : _roots)
        ScanFolder(root, found);
    for (const auto& [target, source] : _built)
        found.push_back(source.lexically_normal());
    std::vector<TuningFileState> files;
    if (!_project.empty()) { // Listed even before it exists: the first edit writes it
        TuningFileState settings;
        settings.kind = TuningFileKind::Settings;
        settings.source = (_project / std::filesystem::path(SettingsFile)).lexically_normal();
        settings.relative = std::string(SettingsFile);
        settings.target = "project";
        if (const auto* previous = FindMutable(settings.source)) {
            settings = *previous;
        } else {
            Read(settings);
        }
        files.push_back(std::move(settings));
    }
    for (const auto& source : found) {
        if (std::ranges::any_of(files, [&](const TuningFileState& file) { return Key(file.source) == Key(source); }))
            continue;
        std::error_code error;
        if (!std::filesystem::is_regular_file(source, error)) continue; // Built once, deleted since
        TuningFileState file;
        file.kind = KindOf(source);
        file.source = source;
        const auto built =
            std::ranges::find_if(_built, [&](const auto& entry) { return Key(entry.second) == Key(source); });
        if (built != _built.end()) file.target = built->first;
        file.relative = _project.empty() ? Utf8(source.filename()) : Utf8(source.lexically_relative(_project));
        if (const auto* previous = FindMutable(source)) { // Keep what is known about it
            file.fields = previous->fields;
            file.problem = previous->problem;
            file.worker = previous->worker;
            file.used = previous->used;
            file.committed = previous->committed;
            file.written = previous->written;
            file.input = previous->input;
            file.committedInput = previous->committedInput;
        } else {
            Read(file);
        }
        files.push_back(std::move(file));
    }
    // The settings file first, then by path.
    std::ranges::sort(files, {}, [](const TuningFileState& file) {
        return std::pair(file.kind != TuningFileKind::Settings, Key(file.source));
    });
    Label(files);
    for (auto& file : files)
        if (file.kind == TuningFileKind::Settings) file.label = "Project settings";
    _files = std::move(files);
}

const TuningFileState* TuningWorkspace::Find(const std::filesystem::path& source) const {
    const auto key = Key(source);
    const auto found =
        std::ranges::find_if(_files, [&](const TuningFileState& file) { return Key(file.source) == key; });
    return found == _files.end() ? nullptr : &*found;
}

const TuningFileState* TuningWorkspace::Settings() const {
    const auto found =
        std::ranges::find(_files, TuningFileKind::Settings, [](const TuningFileState& file) { return file.kind; });
    return found == _files.end() ? nullptr : &*found;
}

TuningFileState* TuningWorkspace::FindMutable(const std::filesystem::path& source) {
    return const_cast<TuningFileState*>(std::as_const(*this).Find(source));
}

std::string TuningWorkspace::Write(TuningFileState& file, const std::string& what, const std::string& before,
                                   const std::string& after) {
    if (after == before) return {};
    if (!WriteBinaryFileAtomic(file.source, after)) {
        Read(file);
        return file.label + ": could not be written";
    }
    Read(file);
    _undo.push_back({file.source, what, before, after});
    if (_undo.size() > MaxHistory) _undo.pop_front();
    _redo.clear();
    return {};
}

std::string TuningWorkspace::Set(const std::filesystem::path& source, const TuningLocation& at,
                                 const TuningValue& value) {
    auto* file = FindMutable(source);
    if (!file) return "Unknown tuning file";
    try {
        const std::string text = Text(*file);
        if (file->kind == TuningFileKind::Settings) {
            const auto values = ParseSettingsValues(text, ProjectSettingsSchema());
            if (const auto* field = Named(values.fields, at.field);
                field && std::ranges::find(values.set, at.field) != values.set.end() && field->values.front() == value)
                return {};
            return Write(*file, Where(*file, at), text,
                         PatchSettingsValue(text, ProjectSettingsSchema(), at.field, value));
        }
        if (file->kind != TuningFileKind::Tuning) return file->label + ": not a tuning file";
        const auto fields = ParseTuningFile(text);
        if (const auto* field =
                Named(fields, at.field)) // Unchanged: no write, so no one-line diff of a respelled number
            if (const auto current = ValueAt(*field, at); current && *current == value) return {};
        return Write(*file, Where(*file, at), text, PatchTuningValue(text, at, value));
    } catch (const TuningFormatError& failure) {
        Read(*file);
        return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        Read(*file);
        return file->label + ": " + failure.what();
    }
}

std::string TuningWorkspace::Resize(const std::filesystem::path& source, const TuningLocation& at,
                                    TuningResize resize) {
    auto* file = FindMutable(source);
    if (!file) return "Unknown tuning file";
    try {
        const std::string text = ReadBinaryFile(file->source, MaxFileBytes);
        const std::string what = file->label + ": " + at.field + (resize == TuningResize::Append ? " +1" : " -1");
        return Write(*file, what, text, ResizeTuningField(text, at, resize));
    } catch (const TuningFormatError& failure) {
        Read(*file);
        return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        Read(*file);
        return file->label + ": " + failure.what();
    }
}

std::string TuningWorkspace::Revert(const std::filesystem::path& source, const std::string& name) {
    auto* file = FindMutable(source);
    if (!file) return "Unknown tuning file";
    const auto* committed = file->CommittedField(name);
    if (!committed) return file->label + ": \"" + name + "\" has no committed value";
    if (file->kind == TuningFileKind::Settings) { // The committed value, or the default when the commit left it out
        try {
            const std::string text = Text(*file);
            const bool wasSet = file->committedSet && std::ranges::find(*file->committedSet, name) != file->committedSet->end();
            const auto patched = PatchSettingsValue(text, ProjectSettingsSchema(), name,
                                                    wasSet ? std::optional(committed->values.front()) : std::nullopt);
            return Write(*file, file->label + ": revert " + name, text, patched);
        } catch (const TuningFormatError& failure) {
            Read(*file);
            return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
        } catch (const std::exception& failure) {
            Read(*file);
            return file->label + ": " + failure.what();
        }
    }
    try {
        const std::string text = ReadBinaryFile(file->source, MaxFileBytes);
        const auto fields = ParseTuningFile(text);
        const auto* current = Named(fields, name);
        if (!current) return file->label + ": no field \"" + name + "\"";
        if (!SameShape(*current, *committed))
            return file->label + ": \"" + name + "\" changed shape since the commit; restore it with git";
        std::string patched = text;
        const auto revert = [&](const TuningLocation& at, const TuningValue& was) {
            if (ValueAt(*current, at) != was) patched = PatchTuningValue(patched, at, was);
        };
        if (!current->table) {
            for (std::size_t i = 0; i < committed->values.size(); ++i)
                revert({name, 0, {}, i}, committed->values[i]);
        } else {
            for (std::size_t r = 0; r < committed->rows.size(); ++r)
                for (std::size_t c = 0; c < committed->columns.size(); ++c)
                    for (std::size_t i = 0; i < committed->rows[r][c].size(); ++i)
                        revert({name, r, committed->columns[c].name, i}, committed->rows[r][c][i]);
        }
        return Write(*file, file->label + ": revert " + name, text, patched);
    } catch (const TuningFormatError& failure) {
        Read(*file);
        return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        Read(*file);
        return file->label + ": " + failure.what();
    }
}

std::string TuningWorkspace::ResetToDefault(const std::filesystem::path& source, const std::string& name) {
    auto* file = FindMutable(source);
    if (!file || file->kind != TuningFileKind::Settings) return "Not a settings file";
    try {
        const std::string text = Text(*file);
        return Write(*file, file->label + ": default " + name, text,
                     PatchSettingsValue(text, ProjectSettingsSchema(), name, std::nullopt));
    } catch (const TuningFormatError& failure) {
        Read(*file);
        return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        Read(*file);
        return file->label + ": " + failure.what();
    }
}

std::string TuningWorkspace::SetKeys(const std::filesystem::path& source, std::size_t player, const std::string& button,
                                     const std::vector<std::string>& keys) {
    auto* file = FindMutable(source);
    if (!file || file->kind != TuningFileKind::InputMap) return "Not an input map";
    try {
        const std::string text = ReadBinaryFile(file->source, MaxFileBytes);
        const std::string what = file->label + ": player " + std::to_string(player + 1) + " " + button;
        return Write(*file, what, text, PatchInputMapKeys(text, player, button, keys));
    } catch (const TuningFormatError& failure) {
        Read(*file);
        return file->label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        Read(*file);
        return file->label + ": " + failure.what();
    }
}

std::optional<TuningHistoryStep> TuningWorkspace::Step(std::deque<Edit>& from, std::deque<Edit>& to, bool undo) {
    if (from.empty()) return std::nullopt;
    auto edit = std::move(from.back());
    from.pop_back();
    TuningHistoryStep step{edit.source, (undo ? "Undid " : "Redid ") + edit.what, false};
    auto* file = FindMutable(edit.source);
    const std::string& expected = undo ? edit.after : edit.before;
    const std::string& wanted = undo ? edit.before : edit.after;
    try {
        if (!file || Text(*file) != expected) {
            step.text = edit.what + " changed since; not " + (undo ? "undone" : "redone");
            return step;
        }
        if (!WriteBinaryFileAtomic(edit.source, wanted)) {
            step.text = edit.what + ": could not be written";
            return step;
        }
    } catch (const std::exception& failure) {
        step.text = edit.what + ": " + failure.what();
        return step;
    }
    Read(*file);
    step.done = true;
    to.push_back(std::move(edit));
    return step;
}

std::optional<TuningHistoryStep> TuningWorkspace::Undo() { return Step(_undo, _redo, true); }
std::optional<TuningHistoryStep> TuningWorkspace::Redo() { return Step(_redo, _undo, false); }

std::string TuningWorkspace::Report(const WorkerTuningStatus& status) {
    if (auto* file = FindMutable(PathOf(status.source))) {
        file->worker = status;
        return file->label;
    }
    return {};
}

void TuningWorkspace::SetUsed(const std::vector<std::filesystem::path>& sources) {
    for (auto& file : _files) {
        file.used = std::ranges::any_of(sources, [&](const auto& source) { return Key(source) == Key(file.source); });
    }
}

void TuningWorkspace::ClearWorkerStatus() {
    for (auto& file : _files) {
        file.worker.reset();
        file.used = false;
    }
}

void TuningWorkspace::SetCommitted(const std::filesystem::path& source, const std::optional<std::string>& text) {
    auto* file = FindMutable(source);
    if (!file) return;
    file->committed.reset();
    file->committedSet.reset();
    file->committedInput.reset();
    if (!text) return;
    try {
        if (file->kind == TuningFileKind::Settings) {
            auto values = ParseSettingsValues(*text, ProjectSettingsSchema());
            file->committed = std::move(values.fields);
            file->committedSet = std::move(values.set);
        } else if (file->kind == TuningFileKind::InputMap) {
            file->committedInput = ParseInputMap(*text);
        } else {
            file->committed = ParseTuningFile(*text);
        }
    } catch (const std::exception&) { // An invalid commit compares with nothing
    }
}
