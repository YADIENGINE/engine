#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Math/FloatRange.h"

// Tuning file format (Docs/Tuning.md): reading, validation, value patching and list / table resizing. Used by the live
// runtime and editor. The build-time generator applies the same rules in Python (Tools/tuning/document.py); shared
// cases in Tools/Tests/TuningCases check both sides. A rule change must update both implementations together.

// Scalar types used to build lists and table columns. Order matches TuningValue's alternatives.
enum class TuningType { Float, Int, Bool, Color, Vec2, Range, Choice };
// A choice's position in its spec's `choices`; the generated enum has the same order.
struct TuningChoice {
    int index = 0;
    friend bool operator==(const TuningChoice&, const TuningChoice&) = default;
};
using TuningValue = std::variant<float, int, bool, Color4, Float2, FloatRange, TuningChoice>;

[[nodiscard]] std::string_view TuningTypeName(TuningType type);

// min / max are validity bounds; applied to each list element and each number in vec2 / range. `choices` only for
// choice; `unit` only for numbers, vec2 and range.
struct TuningSpec {
    TuningType type = TuningType::Float;
    bool list = false;
    std::optional<double> min;
    std::optional<double> max;
    std::vector<std::string> choices;
    std::string unit;
};

struct TuningColumn {
    std::string name;
    std::string note;
    TuningSpec spec;
};

// Value / list field: spec + values (one element for a scalar). Table: columns + rows; rows[row][column] holds that
// cell's values (multiple for a list column). `group` and `note` are editor text; empty when absent.
struct TuningField {
    std::string name;
    int line = 1;
    bool table = false;
    std::string group;
    std::string note;
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
    friend bool operator==(const TuningLocation&, const TuningLocation&) = default;
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

// Changing a length is structural: the running game reports "build to apply" until the next build.
enum class TuningResize { Append, Remove };
// A list field (at.field): appends a copy of its last element, or removes element at.element. A table: appends a copy
// of its last row, or removes row at.row. One entry always remains. The rest of the text stays byte-identical; throws
// TuningFormatError like PatchTuningValue.
[[nodiscard]] std::string ResizeTuningField(std::string_view text, const TuningLocation& at, TuningResize resize);
