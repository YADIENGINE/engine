#include "Editor/Ui/Widgets/Properties.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>

namespace {
std::string Number(double value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return {buffer.data(), result.ptr};
}
std::string WithUnit(std::string text, const PropertySpec& spec) {
    return spec.unit.empty() ? text : text + " " + spec.unit;
}
std::string Hex(Color4 color) {
    std::array<char, 10> text{};
    if (color.a == 255)
        std::snprintf(text.data(), text.size(), "#%02X%02X%02X", color.r, color.g, color.b);
    else
        std::snprintf(text.data(), text.size(), "#%02X%02X%02X%02X", color.r, color.g, color.b, color.a);
    return text.data();
}
} // namespace

std::string PropertyLabel(std::string_view name) {
    std::string label;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        const bool upper = std::isupper(static_cast<unsigned char>(c)) != 0;
        const bool digit = std::isdigit(static_cast<unsigned char>(c)) != 0;
        const bool afterLetter = i > 0 && std::isalpha(static_cast<unsigned char>(name[i - 1])) != 0;
        // A capital starts a word; so does a run of digits after a letter ("tier2Odds" → "Tier 2 odds").
        if (i > 0 && (upper || (digit && afterLetter))) label += ' ';
        label += i == 0 ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                        : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return label;
}

std::string PropertyText(const PropertySpec& spec, const PropertyValue& value) {
    if (const auto* number = std::get_if<double>(&value)) return WithUnit(Number(*number), spec);
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return WithUnit(std::to_string(*integer), spec);
    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? "true" : "false";
    if (const auto* color = std::get_if<Color4>(&value)) return Hex(*color);
    if (const auto* point = std::get_if<Float2>(&value))
        return WithUnit("[" + Number(point->x) + ", " + Number(point->y) + "]", spec);
    if (const auto* range = std::get_if<FloatRange>(&value))
        return WithUnit(Number(range->min) + ".." + Number(range->max), spec);
    if (const auto* text = std::get_if<PropertyString>(&value)) return text->text.empty() ? "(empty)" : text->text;
    const auto index = std::get<PropertyChoice>(value).index;
    return index < spec.choices.size() ? spec.choices[index] : std::string("?");
}

std::string PropertyBounds(const PropertySpec& spec) {
    if (!spec.min && !spec.max) return {};
    return (spec.min ? Number(*spec.min) : "*") + ".." + (spec.max ? Number(*spec.max) : "*");
}

PropertySpec ScenePropertySpec(const SceneParameter& parameter) {
    PropertySpec spec;
    switch (parameter.kind) {
    case SceneParameterKind::Integer: spec.kind = PropertyKind::Integer; break;
    case SceneParameterKind::Float  : spec.kind = PropertyKind::Float; break;
    case SceneParameterKind::Boolean: spec.kind = PropertyKind::Boolean; break;
    case SceneParameterKind::Color  : spec.kind = PropertyKind::Color; break;
    case SceneParameterKind::Choice : spec.kind = PropertyKind::Choice; break;
    }
    if (spec.kind == PropertyKind::Integer || spec.kind == PropertyKind::Float) {
        spec.min = parameter.minimum;
        spec.max = parameter.maximum;
    }
    spec.choices = parameter.choices;
    // A huge integer range makes a slider useless; type it instead.
    const bool wide = spec.kind == PropertyKind::Integer && parameter.maximum - parameter.minimum > 10000.0;
    spec.numbers = parameter.numericInput || wide ? PropertyNumbers::Input : PropertyNumbers::Slider;
    spec.choiceButtons = parameter.choiceButtons;
    spec.choiceButtonColumns = parameter.choiceButtonColumns;
    return spec;
}

PropertyValue ScenePropertyValue(const SceneParameter& parameter, const SceneValue& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return *integer;
    if (const auto* number = std::get_if<double>(&value)) return *number;
    if (const auto* flag = std::get_if<bool>(&value)) return *flag;
    if (const auto* color = std::get_if<Color4>(&value)) return *color;
    const auto& choice = std::get<std::string>(value);
    const auto found = std::ranges::find(parameter.choices, choice);
    return PropertyChoice{static_cast<std::size_t>(found - parameter.choices.begin())};
}

SceneValue SceneValueOf(const SceneParameter& parameter, const PropertyValue& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return *integer;
    if (const auto* number = std::get_if<double>(&value)) return *number;
    if (const auto* flag = std::get_if<bool>(&value)) return *flag;
    if (const auto* color = std::get_if<Color4>(&value)) return *color;
    const auto index = std::get<PropertyChoice>(value).index;
    return index < parameter.choices.size() ? parameter.choices[index] : std::string();
}

PropertySpec TuningPropertySpec(const TuningSpec& spec) {
    PropertySpec property;
    switch (spec.type) {
    case TuningType::Float : property.kind = PropertyKind::Float; break;
    case TuningType::Int   : property.kind = PropertyKind::Integer; break;
    case TuningType::Bool  : property.kind = PropertyKind::Boolean; break;
    case TuningType::Color : property.kind = PropertyKind::Color; break;
    case TuningType::Vec2  : property.kind = PropertyKind::Vec2; break;
    case TuningType::Range : property.kind = PropertyKind::Range; break;
    case TuningType::Choice: property.kind = PropertyKind::Choice; break;
    case TuningType::Text  :
    case TuningType::Path  : property.kind = PropertyKind::Text; break;
    }
    property.min = spec.min;
    property.max = spec.max;
    property.choices = spec.choices;
    property.unit = spec.unit;
    property.numbers = PropertyNumbers::Drag;
    return property;
}

PropertyValue TuningPropertyValue(const TuningValue& value) {
    if (const auto* number = std::get_if<float>(&value)) return static_cast<double>(*number);
    if (const auto* integer = std::get_if<int>(&value)) return static_cast<std::int64_t>(*integer);
    if (const auto* flag = std::get_if<bool>(&value)) return *flag;
    if (const auto* color = std::get_if<Color4>(&value)) return *color;
    if (const auto* point = std::get_if<Float2>(&value)) return *point;
    if (const auto* range = std::get_if<FloatRange>(&value)) return *range;
    if (const auto* text = std::get_if<TuningText>(&value)) return PropertyString{text->text, false};
    if (const auto* path = std::get_if<TuningPath>(&value)) return PropertyString{path->path, true};
    return PropertyChoice{static_cast<std::size_t>(std::max(0, std::get<TuningChoice>(value).index))};
}

TuningValue TuningValueOf(const PropertyValue& value) {
    if (const auto* number = std::get_if<double>(&value)) return static_cast<float>(*number);
    if (const auto* integer = std::get_if<std::int64_t>(&value))
        return static_cast<int>(
            std::clamp<std::int64_t>(*integer, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()));
    if (const auto* flag = std::get_if<bool>(&value)) return *flag;
    if (const auto* color = std::get_if<Color4>(&value)) return *color;
    if (const auto* point = std::get_if<Float2>(&value)) return *point;
    if (const auto* range = std::get_if<FloatRange>(&value)) return *range;
    if (const auto* text = std::get_if<PropertyString>(&value))
        return text->path ? TuningValue{TuningPath{text->text}} : TuningValue{TuningText{text->text}};
    return TuningChoice{static_cast<int>(std::get<PropertyChoice>(value).index)};
}
