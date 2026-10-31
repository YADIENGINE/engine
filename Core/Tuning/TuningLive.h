#pragma once
// Live side of tuning values (Docs/Tuning.md): available only in development builds (ENGINE_TUNING_LIVE).
// Generated struct fields are registered at runtime; when a file changes, Game reads it through PollTuning at
// frame start and applies all values or none. This header is empty in Release; reading / watching code is not compiled.
#if ENGINE_TUNING_LIVE
#include <array>
#include <cstddef>
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
// their own references for each row.
struct TuningFieldRef {
    std::string_view name;
    TuningType type = TuningType::Float;
    bool list = false;
    std::size_t length = 1;
    void* data = nullptr;
    bool table = false;
    std::vector<std::vector<TuningFieldRef>> rows;
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
// changed in type or length, requiring a build. Unreadable: file could not be read.
enum class TuningOutcome { Applied, Invalid, NeedsBuild, Unreadable };

struct TuningReport {
    std::string source; // Dosya yolu, UTF-8
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

// A registered tuning struct; Tuning<T>() creates it as TuningSlot<T>.
class TuningBinding {
public:
    virtual ~TuningBinding() = default;
    [[nodiscard]] virtual std::filesystem::path Source() const = 0;
    [[nodiscard]] virtual TuningReport Apply(std::string_view text) = 0;

protected:
    TuningBinding() = default;

public:
    TuningBinding(const TuningBinding&) = delete;
    TuningBinding& operator=(const TuningBinding&) = delete;
};

// Registers and reads the file once immediately (initial values are compiled defaults, or file values if changed
// since).
void RegisterTuning(TuningBinding& binding);
void UnregisterTuning(TuningBinding& binding);
// Reapplies changed registered files; Game calls this at each frame start (main thread). Checks file status at most
// once every 200 ms.
void PollTuning();
// Report destination; an empty function restores the default (stderr). The default does not report a successful
// initial read.
void SetTuningReporter(std::function<void(const TuningReport&)> reporter);

template <class T> class TuningSlot final : public TuningBinding {
public:
    TuningSlot() { RegisterTuning(*this); }
    ~TuningSlot() override { UnregisterTuning(*this); }

    [[nodiscard]] const T& Value() const { return _value; }
    [[nodiscard]] std::filesystem::path Source() const override { return std::filesystem::path(T::TuningSource); }
    [[nodiscard]] TuningReport Apply(std::string_view text) override {
        std::vector<TuningFieldRef> fields;
        TuningCollector collector(fields);
        _value.VisitTuning(collector);
        return ApplyTuningText(text, fields);
    }

private:
    T _value{};
};
#endif
