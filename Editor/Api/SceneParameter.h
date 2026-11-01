#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

enum class SceneParameterKind { Integer, Float, Boolean, Color, Choice };
enum class SceneApplyMode { Immediate, NextAction, OnReset };

struct SceneColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    bool operator==(const SceneColor&) const = default;
};

using SceneValue = std::variant<std::int64_t, double, bool, SceneColor, std::string>;
using SceneParameters = std::map<std::string, SceneValue>;

struct SceneParameter {
    std::string id;
    std::string label;
    SceneParameterKind kind = SceneParameterKind::Float;
    SceneValue initial = 0.0;
    double minimum = 0.0;
    double maximum = 1.0;
    SceneApplyMode apply = SceneApplyMode::Immediate;
    std::vector<std::string> choices;
    std::string group;
    std::string description;
    bool choiceButtons = false;
    int choiceButtonColumns =
        0; // Zero stretches responsive columns; positive values use compact columns, shrinking to fit.
    bool numericInput = false; // Use a text input instead of a slider for numeric parameters.
    bool advanced = false;     // Groups containing only advanced fields start collapsed.
    std::string applyAction;   // Optional NextAction consumer ID, used only for Inspector guidance.
};

struct SceneAction {
    std::string id;
    std::string label;
    std::string description;
    bool primary = false;
};

// Empty means accepted. Values are rejected, never silently clamped.
[[nodiscard]] std::string ValidateSceneValue(const SceneParameter& parameter, const SceneValue& value);
[[nodiscard]] std::string ValidateSceneParameter(const SceneParameter& parameter);
