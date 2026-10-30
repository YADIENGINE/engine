#include "Tuning/TuningLive.h"
#if ENGINE_TUNING_LIVE
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <optional>
#include <utility>
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr auto PollInterval = std::chrono::milliseconds(200);
constexpr std::size_t MaxFileBytes = 4 * 1024 * 1024;

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
};

Registry& State() {
    static Registry registry;
    return registry;
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
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
        report = entry.binding->Apply(ReadBinaryFile(entry.source, MaxFileBytes));
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

const TuningField* FindField(const std::vector<TuningField>& fields, std::string_view name) {
    const auto found = std::find_if(fields.begin(), fields.end(), [&](const TuningField& f) { return f.name == name; });
    return found == fields.end() ? nullptr : &*found;
}

std::string Shape(TuningType type, bool list, std::size_t length) {
    std::string text(TuningTypeName(type));
    return list ? text + "[" + std::to_string(length) + "]" : text;
}

// Dosyanın yapısı derlenmiş struct'tan farklıysa farkın açıklaması (derleme gerekir).
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
            const std::string was = Shape(ref.type, ref.list, ref.length);
            const std::string now = Shape(field->spec.type, field->spec.list, field->values.size());
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
            const std::string was = Shape(column.type, column.list, column.length);
            const std::string now = Shape(found->spec.type, found->spec.list, field->rows.front()[index].size());
            if (was != now)
                return "column \"" + std::string(column.name) + "\" of " + name + " changed from " + was + " to " + now;
        }
    }
    return std::nullopt;
}

void Write(const TuningFieldRef& ref, const std::vector<TuningValue>& values) {
    for (std::size_t i = 0; i < ref.length; ++i) {
        const TuningValue& value = values[i];
        switch (ref.type) {
        case TuningType::Float: static_cast<float*>(ref.data)[i] = std::get<float>(value); break;
        case TuningType::Int  : static_cast<int*>(ref.data)[i] = std::get<int>(value); break;
        case TuningType::Bool : static_cast<bool*>(ref.data)[i] = std::get<bool>(value); break;
        case TuningType::Color: static_cast<Color4*>(ref.data)[i] = std::get<Color4>(value); break;
        case TuningType::Vec2 : static_cast<Float2*>(ref.data)[i] = std::get<Float2>(value); break;
        case TuningType::Range: static_cast<FloatRange*>(ref.data)[i] = std::get<FloatRange>(value); break;
        }
    }
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

void RegisterTuning(TuningBinding& binding) {
    auto& state = State();
    TuningReport report;
    {
        const std::scoped_lock lock(state.mutex);
        auto& entry = state.entries.emplace_back(Entry{&binding, binding.Source(), std::nullopt});
        report = Load(entry, false);
    }
    Report(report);
}

void UnregisterTuning(TuningBinding& binding) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    std::erase_if(state.entries, [&](const Entry& entry) { return entry.binding == &binding; });
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

void SetTuningReporter(std::function<void(const TuningReport&)> reporter) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    state.reporter = std::move(reporter);
}
#endif
