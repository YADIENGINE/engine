#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Scene/SceneParameter.h"
#include "Tuning/TuningFile.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Math/FloatRange.h"

// One editable value in the Inspector, whatever owns it: a scene parameter (the session's, never saved) or a tuning
// field (saved in its file). Both convert to this model, so one set of widgets edits both (PropertyWidgets.h).

// Order matches PropertyValue's alternatives.
enum class PropertyKind { Float, Integer, Boolean, Color, Vec2, Range, Choice, Text };
struct PropertyChoice {
    std::size_t index = 0;
    friend bool operator==(const PropertyChoice&, const PropertyChoice&) = default;
};
// One line of text; `path` keeps a settings path apart from plain text.
struct PropertyString {
    std::string text;
    bool path = false;
    friend bool operator==(const PropertyString&, const PropertyString&) = default;
};
using PropertyValue = std::variant<double, std::int64_t, bool, Color4, Float2, FloatRange, PropertyChoice, PropertyString>;
// Drag: the number follows the mouse and needs no range (tuning). Slider: across min..max (scene parameters).
// Input: a typed box.
enum class PropertyNumbers { Drag, Slider, Input };

struct PropertySpec {
    PropertyKind kind = PropertyKind::Float;
    std::optional<double> min; // Validity bounds: widgets never go past them
    std::optional<double> max;
    std::vector<std::string> choices;
    std::string unit; // Shown after numbers
    PropertyNumbers numbers = PropertyNumbers::Drag;
    bool choiceButtons = false;
    int choiceButtonColumns = 0;
};

// "glowRadius" → "Glow radius": the label of a tuning field or column.
[[nodiscard]] std::string PropertyLabel(std::string_view name);
// The value as text, for tooltips and agent context: "12.5 px", "#FF4080", "[1, 2]", "Pulse".
[[nodiscard]] std::string PropertyText(const PropertySpec& spec, const PropertyValue& value);
// "0..40", "0..*" or empty without bounds.
[[nodiscard]] std::string PropertyBounds(const PropertySpec& spec);

[[nodiscard]] PropertySpec ScenePropertySpec(const SceneParameter& parameter);
[[nodiscard]] PropertyValue ScenePropertyValue(const SceneParameter& parameter, const SceneValue& value);
[[nodiscard]] SceneValue SceneValueOf(const SceneParameter& parameter, const PropertyValue& value);

[[nodiscard]] PropertySpec TuningPropertySpec(const TuningSpec& spec);
[[nodiscard]] PropertyValue TuningPropertyValue(const TuningValue& value);
[[nodiscard]] TuningValue TuningValueOf(const PropertyValue& value);
