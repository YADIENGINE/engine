#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRange.h"

// Tuning file format (Docs/Tuning.md): reading, validation and single-value patching. Used by the live runtime and
// editor. The build-time generator applies the same rules in Python (Tools/tuning/document.py); shared cases in
// Tools/Tests/TuningCases check both sides. A rule change must update both implementations together.

// Scalar types used to build lists and table columns. Order matches TuningValue's alternatives.
enum class TuningType { Float, Int, Bool, Color, Vec2, Range };
using TuningValue = std::variant<float, int, bool, Color4, Float2, FloatRange>;

[[nodiscard]] std::string_view TuningTypeName(TuningType type);

// min / max are validity bounds; applied to each list element and each number in vec2 / range.
struct TuningSpec {
    TuningType type = TuningType::Float;
    bool list = false;
    std::optional<double> min;
    std::optional<double> max;
};

struct TuningColumn {
    std::string name;
    TuningSpec spec;
};

// Value / list field: spec + values (one element for a scalar). Table: columns + rows; rows[row][column] holds that
// cell's values (multiple for a list column).
struct TuningField {
    std::string name;
    int line = 1;
    bool table = false;
    TuningSpec spec;
    std::vector<TuningValue> values;
    std::vector<TuningColumn> columns;
    std::vector<std::vector<std::vector<TuningValue>>> rows;
};

// Value location: field; row + column for a table (when 'column' is not empty); element for a list.
struct TuningLocation {
    std::string field;
    std::size_t row = 0;
    std::string column;
    std::size_t element = 0;
};

// First error in the file; lines start at 1.
class TuningFormatError final : public std::runtime_error {
public:
    TuningFormatError(int line, const std::string& text);
    [[nodiscard]] int Line() const { return _line; }

private:
    int _line;
};

// Validated fields in file order.
[[nodiscard]] std::vector<TuningField> ParseTuningFile(std::string_view text);
// Changes only that value's spelling in valid 'text'; the rest stays byte-identical. Throws TuningFormatError if
// the file or result is invalid (type mismatch, out of bounds).
[[nodiscard]] std::string PatchTuningValue(std::string_view text, const TuningLocation& at, const TuningValue& value);
