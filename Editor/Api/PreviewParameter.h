#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

enum class PreviewParameterKind { Integer, Float, Boolean, Color, Choice };
enum class PreviewApplyMode { Immediate, NextAction, OnReset };

struct PreviewColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    bool operator==(const PreviewColor&) const = default;
};

using PreviewValue = std::variant<std::int64_t, double, bool, PreviewColor, std::string>;
using PreviewParameters = std::map<std::string, PreviewValue>;

struct PreviewParameter {
    std::string id;
    std::string label;
    PreviewParameterKind kind = PreviewParameterKind::Float;
    PreviewValue initial = 0.0;
    double minimum = 0.0;
    double maximum = 1.0;
    PreviewApplyMode apply = PreviewApplyMode::Immediate;
    std::vector<std::string> choices;
    std::string group;
    std::string description;
    bool choiceButtons = false;
    int choiceButtonColumns = 0; // Zero selects responsive columns; positive values keep a fixed column count.
};

struct PreviewAction {
    std::string id;
    std::string label;
    std::string description;
    bool primary = false;
};

// Empty means accepted. Values are rejected, never silently clamped.
[[nodiscard]] std::string ValidatePreviewValue(const PreviewParameter& parameter, const PreviewValue& value);
[[nodiscard]] std::string ValidatePreviewParameter(const PreviewParameter& parameter);
