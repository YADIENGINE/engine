#include "Tuning/TuningFile.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include "Tuning/Internals/TuningJson.h"

namespace {
constexpr std::array<std::string_view, 6> TypeNames{"float", "int", "bool", "color", "vec2", "range"};
constexpr std::array<std::string_view, 4> FieldKeys{"type", "value", "min", "max"};
constexpr std::array<std::string_view, 3> TableKeys{"type", "columns", "rows"};
constexpr std::array<std::string_view, 3> ColumnKeys{"type", "min", "max"};
// Alan adı üretilen struct'ın üyesi olur; küçük harfle başlayan C++ anahtar sözcükleri ad olamaz.
constexpr auto CppKeywords = std::to_array<std::string_view>(
    {"alignas",     "alignof",   "and",        "and_eq",    "asm",      "auto",         "bitand",
     "bitor",       "bool",      "break",      "case",      "catch",    "char",         "char8_t",
     "char16_t",    "char32_t",  "class",      "compl",     "concept",  "const",        "consteval",
     "constexpr",   "constinit", "const_cast", "continue",  "co_await", "co_return",    "co_yield",
     "decltype",    "default",   "delete",     "do",        "double",   "dynamic_cast", "else",
     "enum",        "explicit",  "export",     "extern",    "false",    "float",        "for",
     "friend",      "goto",      "if",         "inline",    "int",      "long",         "mutable",
     "namespace",   "new",       "noexcept",   "not",       "not_eq",   "nullptr",      "operator",
     "or",          "or_eq",     "private",    "protected", "public",   "register",     "reinterpret_cast",
     "requires",    "return",    "short",      "signed",    "sizeof",   "static",       "static_assert",
     "static_cast", "struct",    "switch",     "template",  "this",     "thread_local", "throw",
     "true",        "try",       "typedef",    "typeid",    "typename", "union",        "unsigned",
     "using",       "virtual",   "void",       "volatile",  "wchar_t",  "while",        "xor",
     "xor_eq"});

[[noreturn]] void Fail(int line, const std::string& text) { throw TuningFormatError(line, text); }

std::string Quoted(std::string_view text) { return "\"" + std::string(text) + "\""; }

template <std::size_t N> std::string Joined(const std::array<std::string_view, N>& words) {
    std::string out;
    for (const auto word : words)
        out += (out.empty() ? "" : ", ") + std::string(word);
    return out;
}

// Sınır mesajında sayı: tamsa tam, değilse en kısa gösterim.
std::string Shown(std::optional<double> number) {
    if (!number) return "*";
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), *number);
    return std::string(buffer.data(), result.ptr);
}

double Number(const TuningJsonNode& node, const std::string& what) {
    if (node.kind != TuningJsonKind::Number) Fail(node.line, what + " must be a number");
    double value = 0.0;
    std::from_chars(node.text.data(), node.text.data() + node.text.size(), value);
    return value;
}

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::optional<Color4> ParseColor(std::string_view text) {
    if ((text.size() != 7 && text.size() != 9) || text[0] != '#') return std::nullopt;
    std::array<std::uint8_t, 4> channels{0, 0, 0, 255};
    for (std::size_t i = 0; i + 1 < text.size(); i += 2) {
        const int high = HexDigit(text[i + 1]);
        const int low = i + 2 < text.size() ? HexDigit(text[i + 2]) : -1;
        if (high < 0 || low < 0) return std::nullopt;
        channels[i / 2] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return Color4{channels[0], channels[1], channels[2], channels[3]};
}

TuningValue Decode(const TuningJsonNode& node, TuningType type, const std::string& what) {
    switch (type) {
    case TuningType::Float: return static_cast<float>(Number(node, what));
    case TuningType::Int  : {
        const bool integer = node.kind == TuningJsonKind::Number && node.text.find_first_of(".eE") == std::string::npos;
        if (!integer) Fail(node.line, what + " must be an integer");
        long long value = 0;
        const auto result = std::from_chars(node.text.data(), node.text.data() + node.text.size(), value);
        if (result.ec != std::errc{} || value < std::numeric_limits<int>::min() ||
            value > std::numeric_limits<int>::max())
            Fail(node.line, what + " does not fit a 32-bit int");
        return static_cast<int>(value);
    }
    case TuningType::Bool:
        if (node.kind != TuningJsonKind::Bool) Fail(node.line, what + " must be true or false");
        return node.boolean;
    case TuningType::Color: {
        const auto color = node.kind == TuningJsonKind::String ? ParseColor(node.text) : std::nullopt;
        if (!color) Fail(node.line, what + " must be a colour \"#RRGGBB\" or \"#RRGGBBAA\"");
        return *color;
    }
    case TuningType::Vec2:
        if (node.kind != TuningJsonKind::Array || node.items.size() != 2) Fail(node.line, what + " must be [x, y]");
        return Float2{static_cast<float>(Number(node.items[0], what)), static_cast<float>(Number(node.items[1], what))};
    case TuningType::Range: {
        const auto* min = node.kind == TuningJsonKind::Object ? node.Find("min") : nullptr;
        const auto* max = node.kind == TuningJsonKind::Object ? node.Find("max") : nullptr;
        if (!min || !max || node.members.size() != 2) Fail(node.line, what + " must be { \"min\": a, \"max\": b }");
        const double low = Number(*min, what);
        const double high = Number(*max, what);
        if (low > high) Fail(node.line, what + " has min greater than max");
        return FloatRange{static_cast<float>(low), static_cast<float>(high)};
    }
    }
    Fail(node.line, what + " has an unknown type");
}

std::vector<double> Numbers(const TuningValue& value, const TuningJsonNode& node) {
    if (std::holds_alternative<int>(value)) return {static_cast<double>(std::get<int>(value))};
    // Float'a çevrilmeden önceki sayılar: sınır, Python üreticisi gibi yazılan değere uygulanır.
    if (std::holds_alternative<float>(value)) return {Number(node, {})};
    if (std::holds_alternative<Float2>(value)) return {Number(node.items[0], {}), Number(node.items[1], {})};
    if (std::holds_alternative<FloatRange>(value))
        return {Number(*node.Find("min"), {}), Number(*node.Find("max"), {})};
    return {};
}

std::vector<TuningValue> Checked(const TuningJsonNode& node, const TuningSpec& spec, const std::string& what) {
    const auto one = [&](const TuningJsonNode& item, const std::string& label) {
        TuningValue value = Decode(item, spec.type, label);
        for (const double number : Numbers(value, item))
            if ((spec.min && number < *spec.min) || (spec.max && number > *spec.max))
                Fail(item.line,
                     label + " " + Shown(number) + " is outside " + Shown(spec.min) + ".." + Shown(spec.max));
        return value;
    };
    if (!spec.list) return {one(node, what)};
    if (node.kind != TuningJsonKind::Array || node.items.empty()) Fail(node.line, what + " must be a non-empty list");
    std::vector<TuningValue> values;
    for (std::size_t i = 0; i < node.items.size(); ++i)
        values.push_back(one(node.items[i], what + "[" + std::to_string(i) + "]"));
    return values;
}

template <std::size_t N>
void CheckKeys(const TuningJsonNode& node, const std::array<std::string_view, N>& allowed, const std::string& where) {
    for (const auto& member : node.members)
        if (std::find(allowed.begin(), allowed.end(), member.key) == allowed.end())
            Fail(member.line, where + " has unknown key " + Quoted(member.key) + " (allowed: " + Joined(allowed) + ")");
}

void CheckName(std::string_view name, int line, const std::string& what) {
    const auto alnum = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };
    if (name.empty() || name[0] < 'a' || name[0] > 'z' || !std::all_of(name.begin(), name.end(), alnum))
        Fail(line, what + " " + Quoted(name) + " must be camelCase: a lowercase letter, then letters and digits");
    if (std::find(CppKeywords.begin(), CppKeywords.end(), name) != CppKeywords.end())
        Fail(line, what + " " + Quoted(name) + " is a C++ keyword");
}

bool Bounded(TuningType type) { return type != TuningType::Bool && type != TuningType::Color; }

TuningSpec Spec(const TuningJsonNode& node, const TuningJsonNode* type, const std::string& where,
                std::optional<TuningType> inferred = std::nullopt) {
    TuningSpec spec;
    if (!type) {
        if (!inferred) Fail(node.line, where + " needs \"type\"");
        spec.type = *inferred;
    } else {
        std::string_view name = type->kind == TuningJsonKind::String ? std::string_view(type->text) : "";
        spec.list = name.ends_with("[]");
        if (spec.list) name.remove_suffix(2);
        const auto found = std::find(TypeNames.begin(), TypeNames.end(), name);
        if (found == TypeNames.end() || name.empty())
            Fail(type->line,
                 where + " has an unknown type; use one of " + Joined(TypeNames) + ", a list \"T[]\" or \"table\"");
        spec.type = static_cast<TuningType>(found - TypeNames.begin());
    }
    const std::string typeText = std::string(TuningTypeName(spec.type)) + (spec.list ? "[]" : "");
    for (const auto* bound : {"min", "max"}) {
        const auto* value = node.Find(bound);
        if (!value) continue;
        if (!Bounded(spec.type)) Fail(value->line, where + " of type " + typeText + " cannot have " + Quoted(bound));
        (std::string_view(bound) == "min" ? spec.min : spec.max) = Number(*value, Quoted(bound));
    }
    if (spec.min && spec.max && *spec.min > *spec.max)
        Fail(node.Find("min")->line, where + " has min greater than max");
    return spec;
}

std::optional<TuningType> Inferred(const TuningJsonNode* value) {
    if (value && value->kind == TuningJsonKind::Number) return TuningType::Float;
    if (value && value->kind == TuningJsonKind::Bool) return TuningType::Bool;
    return std::nullopt;
}

TuningField ValueField(const TuningJsonMember& member) {
    const auto& node = member.value;
    const std::string where = Quoted(member.key);
    CheckKeys(node, FieldKeys, where);
    const auto* value = node.Find("value");
    if (!value) Fail(node.line, where + " needs \"value\"");
    TuningField field{member.key, member.line};
    field.spec = Spec(node, node.Find("type"), where, Inferred(value));
    field.values = Checked(*value, field.spec, where + " value");
    return field;
}

TuningField TableField(const TuningJsonMember& member) {
    const auto& node = member.value;
    const std::string where = "table " + Quoted(member.key);
    CheckKeys(node, TableKeys, where);
    for (const auto* required : {"columns", "rows"})
        if (!node.Find(required)) Fail(node.line, where + " needs " + Quoted(required));
    const auto& columns = *node.Find("columns");
    const auto& rows = *node.Find("rows");
    if (columns.kind != TuningJsonKind::Object || columns.members.empty())
        Fail(columns.line, where + " \"columns\" must be a non-empty object");
    TuningField field{member.key, member.line, true};
    for (const auto& column : columns.members) {
        CheckName(column.key, column.line, "column");
        const std::string label = "column " + Quoted(column.key);
        if (column.value.kind != TuningJsonKind::Object) Fail(column.value.line, label + " must be an object");
        CheckKeys(column.value, ColumnKeys, label);
        field.columns.push_back({column.key, Spec(column.value, column.value.Find("type"), label)});
    }
    if (rows.kind != TuningJsonKind::Array || rows.items.empty())
        Fail(rows.line, where + " \"rows\" must be a non-empty list");
    for (std::size_t index = 0; index < rows.items.size(); ++index) {
        const auto& row = rows.items[index];
        const std::string label = where + " row " + std::to_string(index);
        if (row.kind != TuningJsonKind::Object) Fail(row.line, label + " must be an object");
        for (const auto& cell : row.members) {
            const auto known = std::find_if(field.columns.begin(), field.columns.end(),
                                            [&](const TuningColumn& column) { return column.name == cell.key; });
            if (known == field.columns.end()) Fail(cell.line, label + " has unknown column " + Quoted(cell.key));
        }
        auto& values = field.rows.emplace_back();
        for (std::size_t c = 0; c < field.columns.size(); ++c) {
            const auto& column = field.columns[c];
            const auto* cell = row.Find(column.name);
            if (!cell) Fail(row.line, label + " is missing column " + Quoted(column.name));
            const std::string cellLabel = "row " + std::to_string(index) + " " + Quoted(column.name);
            values.push_back(Checked(*cell, column.spec, cellLabel));
            const std::size_t expected = field.rows.front()[c].size();
            if (column.spec.list && values.back().size() != expected)
                Fail(cell->line, cellLabel + " has " + std::to_string(values.back().size()) + " values; row 0 has " +
                                     std::to_string(expected));
        }
    }
    return field;
}

std::vector<TuningField> Fields(const TuningJsonNode& root) {
    if (root.kind != TuningJsonKind::Object) Fail(root.line, "a tuning file is a JSON object of fields");
    if (root.members.empty()) Fail(root.line, "a tuning file needs at least one field");
    std::vector<TuningField> fields;
    for (const auto& member : root.members) {
        CheckName(member.key, member.line, "field");
        if (member.value.kind != TuningJsonKind::Object)
            Fail(member.value.line, Quoted(member.key) + " must be an object such as { \"value\": 1.0 }");
        const auto* type = member.value.Find("type");
        const bool table = type && type->kind == TuningJsonKind::String && type->text == "table";
        fields.push_back(table ? TableField(member) : ValueField(member));
    }
    return fields;
}

std::string FormatNumber(float value) {
    if (!std::isfinite(value)) throw TuningFormatError(1, "a tuning number must be finite");
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    std::string text(buffer.data(), result.ptr);
    if (text.find_first_of(".e") == std::string::npos) text += ".0";
    return text;
}

std::string FormatHex(std::uint8_t channel) {
    constexpr std::string_view Digits = "0123456789ABCDEF";
    return {Digits[channel >> 4], Digits[channel & 0x0F]};
}

// Kanonik satır içi yazım (Tools/tuning/layout.py ile aynı).
std::string Format(const TuningValue& value) {
    if (const auto* number = std::get_if<float>(&value)) return FormatNumber(*number);
    if (const auto* integer = std::get_if<int>(&value)) return std::to_string(*integer);
    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const auto* color = std::get_if<Color4>(&value)) {
        std::string text = "\"#" + FormatHex(color->r) + FormatHex(color->g) + FormatHex(color->b);
        if (color->a != 255) text += FormatHex(color->a);
        return text + "\"";
    }
    if (const auto* point = std::get_if<Float2>(&value))
        return "[" + FormatNumber(point->x) + ", " + FormatNumber(point->y) + "]";
    const auto& range = std::get<FloatRange>(value);
    return "{ \"min\": " + FormatNumber(range.min) + ", \"max\": " + FormatNumber(range.max) + " }";
}
} // namespace

TuningFormatError::TuningFormatError(int line, const std::string& text)
    : std::runtime_error(text)
    , _line(line) {}

std::string_view TuningTypeName(TuningType type) { return TypeNames[static_cast<std::size_t>(type)]; }

std::vector<TuningField> ParseTuningFile(std::string_view text) { return Fields(ParseTuningJson(text)); }

std::string PatchTuningValue(std::string_view text, const TuningLocation& at, const TuningValue& value) {
    const TuningJsonNode root = ParseTuningJson(text);
    const auto fields = Fields(root);
    const auto field =
        std::find_if(fields.begin(), fields.end(), [&](const TuningField& f) { return f.name == at.field; });
    if (field == fields.end()) Fail(1, "no field " + Quoted(at.field));
    const auto& fieldNode = *root.Find(at.field);
    const TuningJsonNode* target = nullptr;
    TuningSpec spec = field->spec;
    if (field->table) {
        const auto column = std::find_if(field->columns.begin(), field->columns.end(),
                                         [&](const TuningColumn& c) { return c.name == at.column; });
        const auto& rows = fieldNode.Find("rows")->items;
        if (column == field->columns.end() || at.row >= rows.size())
            Fail(fieldNode.line, "no cell " + Quoted(at.column) + " in row " + std::to_string(at.row));
        spec = column->spec;
        target = rows[at.row].Find(at.column);
    } else {
        if (!at.column.empty()) Fail(fieldNode.line, Quoted(at.field) + " is not a table");
        target = fieldNode.Find("value");
    }
    if (spec.list) {
        if (at.element >= target->items.size())
            Fail(target->line, "no element " + std::to_string(at.element) + " in " + Quoted(at.field));
        target = &target->items[at.element];
    }
    if (value.index() != static_cast<std::size_t>(spec.type))
        Fail(target->line, "a " + std::string(TuningTypeName(spec.type)) + " value is expected");
    std::string patched =
        std::string(text.substr(0, target->begin)) + Format(value) + std::string(text.substr(target->end));
    (void)ParseTuningFile(patched); // Sınır dışı değer burada reddedilir; dosya yazılmaz
    return patched;
}
