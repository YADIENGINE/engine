#pragma once
// Live side of tuning values (Docs/Tuning.md). Generated struct fields are registered at runtime; when a file changes,
// Game reads it through PollTuning at frame start and applies all values or none. The engine always carries this code,
// so one set of libraries serves both modes; only code built with ENGINE_TUNING_LIVE registers anything.
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Tuning/TuningFile.h"

template <class T> struct TuningTypeOf;
template <> struct TuningTypeOf<float> {
    static constexpr TuningType Value = TuningType::Float;
};
template <> struct TuningTypeOf<int> {
    static constexpr TuningType Value = TuningType::Int;
};
template <> struct TuningTypeOf<bool> {
    static constexpr TuningType Value = TuningType::Bool;
};
template <> struct TuningTypeOf<Color4> {
    static constexpr TuningType Value = TuningType::Color;
};
template <> struct TuningTypeOf<Float2> {
    static constexpr TuningType Value = TuningType::Vec2;
};
template <> struct TuningTypeOf<FloatRange> {
    static constexpr TuningType Value = TuningType::Range;
};

// A field in the compiled struct. Value / list: 'data' points to 'length' consecutive elements. Table: columns have
// their own references for each row. Choice: elements are `enum class : int`, `choices` their names in order.
struct TuningFieldRef {
    std::string_view name;
    TuningType type = TuningType::Float;
    bool list = false;
    std::size_t length = 1;
    void* data = nullptr;
    bool table = false;
    std::vector<std::vector<TuningFieldRef>> rows;
    std::span<const std::string_view> choices;
};

// Visitor for generated VisitTuning: converts fields to a TuningFieldRef list.
class TuningCollector {
public:
    explicit TuningCollector(std::vector<TuningFieldRef>& fields)
        : _fields(fields) {}

    template <class T> void Field(std::string_view name, T& value) {
        _fields.push_back({name, TuningTypeOf<T>::Value, false, 1, &value});
    }
    template <class T, std::size_t N> void Field(std::string_view name, std::array<T, N>& values) {
        _fields.push_back({name, TuningTypeOf<T>::Value, true, N, values.data()});
    }
    template <class E, std::size_t C>
    void Choice(std::string_view name, E& value, const std::array<std::string_view, C>& names) {
        static_assert(sizeof(E) == sizeof(int), "a tuning choice is an enum class : int");
        _fields.push_back({name, TuningType::Choice, false, 1, &value, false, {}, names});
    }
    template <class E, std::size_t N, std::size_t C>
    void Choice(std::string_view name, std::array<E, N>& values, const std::array<std::string_view, C>& names) {
        static_assert(sizeof(E) == sizeof(int), "a tuning choice is an enum class : int");
        _fields.push_back({name, TuningType::Choice, true, N, values.data(), false, {}, names});
    }
    template <class Row, std::size_t N> void Table(std::string_view name, std::array<Row, N>& rows) {
        TuningFieldRef field{name};
        field.table = true;
        for (auto& row : rows) {
            TuningCollector columns(field.rows.emplace_back());
            row.VisitTuning(columns);
        }
        _fields.push_back(std::move(field));
    }

private:
    std::vector<TuningFieldRef>& _fields;
};

// Applied: values written. Invalid: invalid file, last valid values remain. NeedsBuild: a field was added / removed /
// changed in type, choices or length, requiring a build. Unreadable: file could not be read.
enum class TuningOutcome { Applied, Invalid, NeedsBuild, Unreadable };

struct TuningReport {
    std::string source; // Absolute file path, UTF-8
    TuningOutcome outcome = TuningOutcome::Applied;
    int line = 0; // Error line for Invalid
    std::string text;
    bool reload = false; // false: initial startup read
};

// Validates text against compiled fields; writes all values if valid with matching structure, otherwise changes
// none. The returned report's source / reload fields are empty.
[[nodiscard]] TuningReport ApplyTuningText(std::string_view text, std::span<const TuningFieldRef> fields);
// One line in "file:line: ..." format.
[[nodiscard]] std::string DescribeTuningReport(const TuningReport& report);

// A registered tuning struct; Tuning<T>() creates it as TuningSlot<T>. `relative` is the file's path from the project
// root (generated TuningSource).
class TuningBinding {
public:
    virtual ~TuningBinding() = default;
    [[nodiscard]] std::u8string_view Relative() const { return _relative; }
    [[nodiscard]] virtual std::vector<TuningFieldRef> Fields() = 0;
    // Marks the binding used in the current use period (BeginTuningUse); cheap when already marked.
    void NoteUse() {
        if (_used.load(std::memory_order_relaxed) != CurrentTuningUse()) MarkUsed();
    }

protected:
    explicit TuningBinding(std::u8string_view relative)
        : _relative(relative) {}

private:
    [[nodiscard]] static std::uint32_t CurrentTuningUse();
    void MarkUsed();
    std::u8string_view _relative;
    std::atomic<std::uint32_t> _used{0};

public:
    TuningBinding(const TuningBinding&) = delete;
    TuningBinding& operator=(const TuningBinding&) = delete;
};

// The folder tuning sources are relative to. Unless set, the nearest folder holding Project.json above the executable,
// else above the working directory, else the working directory. Moving the project needs no rebuild.
void SetTuningRoot(const std::filesystem::path& root);
[[nodiscard]] std::filesystem::path TuningRoot();

// Registers and reads the file once immediately (initial values are compiled defaults, or file values if changed
// since).
void RegisterTuning(TuningBinding& binding);
void UnregisterTuning(TuningBinding& binding);
// Reapplies changed registered files; Game calls this at each frame start (main thread). Checks file status at most
// once every 200 ms.
void PollTuning();
// Rereads one file now (absolute path), reporting the outcome; the editor calls it right after writing.
void ReloadTuning(const std::filesystem::path& source);
// Writes one value of a registered file (absolute path) in memory, for the editor's preview while a value is dragged;
// the file is untouched, and its next read replaces the value. Empty: applied; otherwise why not.
[[nodiscard]] std::string PreviewTuning(const std::filesystem::path& source, const TuningLocation& at,
                                        const TuningValue& value);
// Report destination; an empty function restores the default (stderr). The default does not report a successful
// initial read.
void SetTuningReporter(std::function<void(const TuningReport&)> reporter);

// Use periods: the editor shows which files the open scene reads. A period starts empty; Tuning<T>() adds its file.
void BeginTuningUse();
// Absolute paths of the files read in this period, in the order first read.
[[nodiscard]] std::vector<std::filesystem::path> TuningUsed();
// Changes whenever the period starts or its list grows.
[[nodiscard]] std::uint64_t TuningUseRevision();

// A file's applied values, for tapes: FNV-1a 64 over field names and value bits. `source` is the project-relative path.
struct TuningDigest {
    std::string source;
    std::uint64_t hash = 0;
    friend bool operator==(const TuningDigest&, const TuningDigest&) = default;
};
// Every registered file, sorted by source.
[[nodiscard]] std::vector<TuningDigest> TuningDigests();

template <class T> class TuningSlot final : public TuningBinding {
public:
    TuningSlot()
        : TuningBinding(T::TuningSource) {
        RegisterTuning(*this);
    }
    ~TuningSlot() override { UnregisterTuning(*this); }

    [[nodiscard]] const T& Use() {
        NoteUse();
        return _value;
    }
    [[nodiscard]] const T& Value() const { return _value; }
    [[nodiscard]] std::vector<TuningFieldRef> Fields() override {
        std::vector<TuningFieldRef> fields;
        TuningCollector collector(fields);
        _value.VisitTuning(collector);
        return fields;
    }

private:
    T _value{};
};
