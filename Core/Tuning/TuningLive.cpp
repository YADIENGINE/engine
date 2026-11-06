#include "Tuning/TuningLive.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <optional>
#include <utility>
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr auto PollInterval = std::chrono::milliseconds(200);
constexpr std::size_t MaxFileBytes = 4 * 1024 * 1024;
constexpr std::uint64_t FnvOffset = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime = 1099511628211ull;

struct Entry {
    TuningBinding* binding = nullptr;
    std::filesystem::path source;
    std::optional<std::filesystem::file_time_type> written;
};

struct Registry {
    std::mutex mutex;
    std::vector<Entry> entries;
    std::function<void(const TuningReport&)> reporter;
    std::chrono::steady_clock::time_point polledAt{};
    std::optional<std::filesystem::path> root;
    std::vector<TuningBinding*> used; // This period's, in first-use order
    std::uint64_t useRevision = 0;
};

Registry& State() {
    static Registry registry;
    return registry;
}

// 0 means "never used": a binding starts there, so period 1 marks it.
std::atomic<std::uint32_t>& UsePeriod() {
    static std::atomic<std::uint32_t> period{1};
    return period;
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}

// Paths from the generator and from the editor may differ in separators and, on Windows, letter case.
std::wstring Key(const std::filesystem::path& file) {
    std::wstring key = std::filesystem::absolute(file).lexically_normal().generic_wstring();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
#endif
    return key;
}

std::optional<std::filesystem::path> ProjectAbove(std::filesystem::path folder) {
    std::error_code error;
    while (!folder.empty()) {
        if (std::filesystem::is_regular_file(folder / "Project.json", error)) return folder;
        const auto parent = folder.parent_path();
        if (parent == folder) break;
        folder = parent;
    }
    return std::nullopt;
}

std::filesystem::path ExecutableDirectory() {
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return {};
    path.resize(length);
    return std::filesystem::path(path).parent_path();
#else
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error).parent_path();
#endif
}

std::filesystem::path FindRoot() {
    if (auto root = ProjectAbove(ExecutableDirectory())) return *root;
    std::error_code error;
    const auto current = std::filesystem::current_path(error);
    if (auto root = ProjectAbove(current)) return *root;
    return current;
}

// Under the registry's lock.
const std::filesystem::path& Root(Registry& state) {
    if (!state.root) state.root = FindRoot();
    return *state.root;
}

std::filesystem::path Resolve(Registry& state, std::u8string_view relative) {
    return (Root(state) / std::filesystem::path(relative)).lexically_normal();
}

std::optional<std::filesystem::file_time_type> WriteTime(const std::filesystem::path& source) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(source, error);
    return error ? std::nullopt : std::optional(time);
}

TuningReport Load(Entry& entry, bool reload) {
    entry.written = WriteTime(entry.source);
    TuningReport report;
    try {
        report = ApplyTuningText(ReadBinaryFile(entry.source, MaxFileBytes), entry.binding->Fields());
    } catch (const std::exception& error) {
        report = {{}, TuningOutcome::Unreadable, 0, error.what()};
    }
    report.source = Utf8(entry.source);
    report.reload = reload;
    return report;
}

void Report(const TuningReport& report) {
    std::function<void(const TuningReport&)> reporter;
    {
        auto& state = State();
        const std::scoped_lock lock(state.mutex);
        reporter = state.reporter;
    }
    if (reporter) {
        reporter(report);
    } else if (report.reload || report.outcome != TuningOutcome::Applied) {
        std::fprintf(stderr, "%s\n", DescribeTuningReport(report).c_str());
    }
}

Entry* FindEntry(Registry& state, const std::filesystem::path& source) {
    const auto key = Key(source);
    const auto found =
        std::find_if(state.entries.begin(), state.entries.end(), [&](const Entry& e) { return Key(e.source) == key; });
    return found == state.entries.end() ? nullptr : &*found;
}

const TuningField* FindField(const std::vector<TuningField>& fields, std::string_view name) {
    const auto found = std::find_if(fields.begin(), fields.end(), [&](const TuningField& f) { return f.name == name; });
    return found == fields.end() ? nullptr : &*found;
}

std::string Shape(TuningType type, bool list, std::size_t length, std::span<const std::string_view> choices) {
    std::string text(TuningTypeName(type));
    if (type == TuningType::Choice) {
        text += "(";
        for (std::size_t i = 0; i < choices.size(); ++i)
            text += (i ? "|" : "") + std::string(choices[i]);
        text += ")";
    }
    return list ? text + "[" + std::to_string(length) + "]" : text;
}

std::string Shape(const TuningSpec& spec, std::size_t length) {
    const std::vector<std::string_view> choices(spec.choices.begin(), spec.choices.end());
    return Shape(spec.type, spec.list, length, choices);
}

// The difference when the file's structure no longer matches the compiled struct (a build is needed).
std::optional<std::string> Difference(const std::vector<TuningField>& file, std::span<const TuningFieldRef> compiled) {
    for (const auto& field : file)
        if (std::none_of(compiled.begin(), compiled.end(),
                         [&](const TuningFieldRef& c) { return c.name == field.name; }))
            return "new field \"" + field.name + "\"";
    for (const auto& ref : compiled) {
        const std::string name = "\"" + std::string(ref.name) + "\"";
        const auto* field = FindField(file, ref.name);
        if (!field) return "field " + name + " was removed";
        if (field->table != ref.table) return "field " + name + " changed between value and table";
        if (!ref.table) {
            const std::string was = Shape(ref.type, ref.list, ref.length, ref.choices);
            const std::string now = Shape(field->spec, field->values.size());
            if (was != now) return "field " + name + " changed from " + was + " to " + now;
            continue;
        }
        if (field->rows.size() != ref.rows.size())
            return "table " + name + " has " + std::to_string(field->rows.size()) + " rows instead of " +
                   std::to_string(ref.rows.size());
        const auto& columns = ref.rows.front();
        if (field->columns.size() != columns.size()) return "table " + name + " changed its columns";
        for (const auto& column : columns) {
            const auto found = std::find_if(field->columns.begin(), field->columns.end(),
                                            [&](const TuningColumn& c) { return c.name == column.name; });
            if (found == field->columns.end())
                return "table " + name + " lost column \"" + std::string(column.name) + "\"";
            const auto index = static_cast<std::size_t>(found - field->columns.begin());
            const std::string was = Shape(column.type, column.list, column.length, column.choices);
            const std::string now = Shape(found->spec, field->rows.front()[index].size());
            if (was != now)
                return "column \"" + std::string(column.name) + "\" of " + name + " changed from " + was + " to " + now;
        }
    }
    return std::nullopt;
}

void WriteOne(const TuningFieldRef& ref, std::size_t i, const TuningValue& value) {
    switch (ref.type) {
    case TuningType::Float : static_cast<float*>(ref.data)[i] = std::get<float>(value); break;
    case TuningType::Int   : static_cast<int*>(ref.data)[i] = std::get<int>(value); break;
    case TuningType::Bool  : static_cast<bool*>(ref.data)[i] = std::get<bool>(value); break;
    case TuningType::Color : static_cast<Color4*>(ref.data)[i] = std::get<Color4>(value); break;
    case TuningType::Vec2  : static_cast<Float2*>(ref.data)[i] = std::get<Float2>(value); break;
    case TuningType::Range : static_cast<FloatRange*>(ref.data)[i] = std::get<FloatRange>(value); break;
    case TuningType::Choice: { // An enum class : int; copied as bytes, not through an int*
        const int index = std::get<TuningChoice>(value).index;
        std::memcpy(static_cast<std::byte*>(ref.data) + i * sizeof(int), &index, sizeof(int));
        break;
    }
    case TuningType::Text: // Settings schemas only; a tuning struct has none
    case TuningType::Path: break;
    }
}

void Write(const TuningFieldRef& ref, const std::vector<TuningValue>& values) {
    for (std::size_t i = 0; i < ref.length; ++i)
        WriteOne(ref, i, values[i]);
}

void Hash(std::uint64_t& hash, const void* data, std::size_t bytes) {
    const auto* byte = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < bytes; ++i) {
        hash ^= byte[i];
        hash *= FnvPrime;
    }
}

void HashText(std::uint64_t& hash, std::string_view text) {
    Hash(hash, text.data(), text.size());
    const unsigned char end = 0;
    Hash(hash, &end, 1);
}

// Field by field and member by member: never raw struct bytes, whose padding is not part of the value.
void HashField(std::uint64_t& hash, const TuningFieldRef& ref) {
    HashText(hash, ref.name);
    if (ref.table) {
        for (const auto& row : ref.rows)
            for (const auto& column : row)
                HashField(hash, column);
        return;
    }
    for (std::size_t i = 0; i < ref.length; ++i) {
        switch (ref.type) {
        case TuningType::Float: Hash(hash, static_cast<const float*>(ref.data) + i, sizeof(float)); break;
        case TuningType::Int  : Hash(hash, static_cast<const int*>(ref.data) + i, sizeof(int)); break;
        case TuningType::Bool : {
            const unsigned char flag = static_cast<const bool*>(ref.data)[i] ? 1 : 0;
            Hash(hash, &flag, 1);
            break;
        }
        case TuningType::Color: {
            const auto& color = static_cast<const Color4*>(ref.data)[i];
            const std::array<std::uint8_t, 4> channels{color.r, color.g, color.b, color.a};
            Hash(hash, channels.data(), channels.size());
            break;
        }
        case TuningType::Vec2: {
            const auto& point = static_cast<const Float2*>(ref.data)[i];
            Hash(hash, &point.x, sizeof(float));
            Hash(hash, &point.y, sizeof(float));
            break;
        }
        case TuningType::Range: {
            const auto& range = static_cast<const FloatRange*>(ref.data)[i];
            Hash(hash, &range.min, sizeof(float));
            Hash(hash, &range.max, sizeof(float));
            break;
        }
        case TuningType::Choice:
            Hash(hash, static_cast<const std::byte*>(ref.data) + i * sizeof(int), sizeof(int));
            break;
        case TuningType::Text: // Settings schemas only; a tuning struct has none
        case TuningType::Path: break;
        }
    }
}

std::string Preview(std::vector<TuningFieldRef> fields, const TuningLocation& at, const TuningValue& value) {
    const auto field =
        std::find_if(fields.begin(), fields.end(), [&](const TuningFieldRef& f) { return f.name == at.field; });
    if (field == fields.end()) return "no field \"" + at.field + "\" in the running build";
    const TuningFieldRef* target = &*field;
    if (field->table) {
        if (at.row >= field->rows.size()) return "no row " + std::to_string(at.row);
        const auto& row = field->rows[at.row];
        const auto column =
            std::find_if(row.begin(), row.end(), [&](const TuningFieldRef& c) { return c.name == at.column; });
        if (column == row.end()) return "no column \"" + at.column + "\"";
        target = &*column;
    } else if (!at.column.empty()) {
        return "\"" + at.field + "\" is not a table";
    }
    if (at.element >= target->length) return "no element " + std::to_string(at.element);
    if (value.index() != static_cast<std::size_t>(target->type)) return "a different type is compiled";
    if (const auto* choice = std::get_if<TuningChoice>(&value);
        choice && (choice->index < 0 || static_cast<std::size_t>(choice->index) >= target->choices.size()))
        return "no such choice";
    WriteOne(*target, at.element, value);
    return {};
}
} // namespace

TuningReport ApplyTuningText(std::string_view text, std::span<const TuningFieldRef> fields) {
    std::vector<TuningField> file;
    try {
        file = ParseTuningFile(text);
    } catch (const TuningFormatError& error) {
        return {{}, TuningOutcome::Invalid, error.Line(), error.what()};
    }
    if (auto difference = Difference(file, fields))
        return {{}, TuningOutcome::NeedsBuild, 0, *difference + "; build to apply"};
    for (const auto& ref : fields) {
        const auto& field = *FindField(file, ref.name);
        if (!ref.table) {
            Write(ref, field.values);
            continue;
        }
        for (std::size_t row = 0; row < ref.rows.size(); ++row)
            for (const auto& column : ref.rows[row]) {
                const auto found = std::find_if(field.columns.begin(), field.columns.end(),
                                                [&](const TuningColumn& c) { return c.name == column.name; });
                Write(column, field.rows[row][static_cast<std::size_t>(found - field.columns.begin())]);
            }
    }
    return {};
}

std::string DescribeTuningReport(const TuningReport& report) {
    switch (report.outcome) {
    case TuningOutcome::Applied: return report.source + ": tuning applied";
    case TuningOutcome::Invalid:
        return report.source + ":" + std::to_string(report.line) + ": tuning not applied: " + report.text;
    case TuningOutcome::NeedsBuild: return report.source + ": tuning not applied: " + report.text;
    case TuningOutcome::Unreadable: return report.source + ": tuning file unreadable: " + report.text;
    }
    return report.source;
}

std::uint32_t TuningBinding::CurrentTuningUse() { return UsePeriod().load(std::memory_order_relaxed); }

void TuningBinding::MarkUsed() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    const auto period = CurrentTuningUse();
    if (_used.exchange(period, std::memory_order_relaxed) == period) return;
    state.used.push_back(this);
    ++state.useRevision;
}

void SetTuningRoot(const std::filesystem::path& root) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    state.root = root;
    for (auto& entry : state.entries)
        entry.source = Resolve(state, entry.binding->Relative());
}

std::filesystem::path TuningRoot() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    return Root(state);
}

void RegisterTuning(TuningBinding& binding) {
    auto& state = State();
    TuningReport report;
    {
        const std::scoped_lock lock(state.mutex);
        auto& entry = state.entries.emplace_back(Entry{&binding, Resolve(state, binding.Relative()), std::nullopt});
        report = Load(entry, false);
    }
    Report(report);
}

void UnregisterTuning(TuningBinding& binding) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    std::erase_if(state.entries, [&](const Entry& entry) { return entry.binding == &binding; });
    std::erase(state.used, &binding);
}

void PollTuning() {
    auto& state = State();
    std::vector<TuningReport> reports;
    {
        const std::scoped_lock lock(state.mutex);
        const auto now = std::chrono::steady_clock::now();
        if (now - state.polledAt < PollInterval) return;
        state.polledAt = now;
        for (auto& entry : state.entries)
            if (WriteTime(entry.source) != entry.written) reports.push_back(Load(entry, true));
    }
    for (const auto& report : reports)
        Report(report);
}

void ReloadTuning(const std::filesystem::path& source) {
    auto& state = State();
    std::optional<TuningReport> report;
    {
        const std::scoped_lock lock(state.mutex);
        if (auto* entry = FindEntry(state, source)) report = Load(*entry, true);
    }
    if (report) Report(*report);
}

std::string PreviewTuning(const std::filesystem::path& source, const TuningLocation& at, const TuningValue& value) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    auto* entry = FindEntry(state, source);
    if (!entry) return "the running scene does not use this file";
    return Preview(entry->binding->Fields(), at, value);
}

void SetTuningReporter(std::function<void(const TuningReport&)> reporter) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    state.reporter = std::move(reporter);
}

void BeginTuningUse() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    auto next = UsePeriod().load() + 1;
    if (next == 0) next = 1; // 0 is "never used"
    UsePeriod().store(next);
    state.used.clear();
    ++state.useRevision;
}

std::vector<std::filesystem::path> TuningUsed() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    std::vector<std::filesystem::path> sources;
    for (auto* binding : state.used)
        sources.push_back(Resolve(state, binding->Relative()));
    return sources;
}

std::uint64_t TuningUseRevision() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    return state.useRevision;
}

std::vector<TuningDigest> TuningDigests() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    std::vector<TuningDigest> digests;
    for (const auto& entry : state.entries) {
        const auto relative = entry.binding->Relative();
        TuningDigest digest{{relative.begin(), relative.end()}, FnvOffset};
        for (const auto& field : entry.binding->Fields())
            HashField(digest.hash, field);
        digests.push_back(std::move(digest));
    }
    std::ranges::sort(digests, {}, &TuningDigest::source);
    return digests;
}
