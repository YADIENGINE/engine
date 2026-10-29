#include "Editor/Api/PreviewParameter.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

std::string ValidatePreviewParameter(const PreviewParameter& parameter) {
    if (parameter.id.empty() || !std::isfinite(parameter.minimum) || !std::isfinite(parameter.maximum) ||
        parameter.minimum > parameter.maximum || parameter.choiceButtonColumns < 0 ||
        parameter.choiceButtonColumns > 64 ||
        (!parameter.applyAction.empty() && parameter.apply != PreviewApplyMode::NextAction))
        return "Invalid parameter metadata: " + parameter.id;
    if (parameter.kind == PreviewParameterKind::Integer) {
        const double lowest = static_cast<double>(std::numeric_limits<std::int64_t>::min());
        if (parameter.minimum < lowest || parameter.maximum >= -lowest ||
            std::floor(parameter.minimum) != parameter.minimum || std::floor(parameter.maximum) != parameter.maximum)
            return "Integer range is not representable: " + parameter.id;
    }
    if (parameter.kind == PreviewParameterKind::Choice) {
        std::set<std::string> choices;
        for (const auto& choice : parameter.choices)
            if (choice.empty() || !choices.insert(choice).second) return "Invalid parameter choices: " + parameter.id;
    }
    return ValidatePreviewValue(parameter, parameter.initial);
}

std::string ValidatePreviewValue(const PreviewParameter& parameter, const PreviewValue& value) {
    switch (parameter.kind) {
    case PreviewParameterKind::Integer:
        if (const auto* number = std::get_if<std::int64_t>(&value)) {
            if (static_cast<double>(*number) >= parameter.minimum && static_cast<double>(*number) <= parameter.maximum)
                return {};
        }
        break;
    case PreviewParameterKind::Float:
        if (const auto* number = std::get_if<double>(&value)) {
            if (std::isfinite(*number) && *number >= parameter.minimum && *number <= parameter.maximum) return {};
        }
        break;
    case PreviewParameterKind::Boolean:
        if (std::holds_alternative<bool>(value)) return {};
        break;
    case PreviewParameterKind::Color:
        if (const auto* color = std::get_if<PreviewColor>(&value)) {
            const auto valid = [](float channel) {
                return std::isfinite(channel) && channel >= 0.0f && channel <= 1.0f;
            };
            if (valid(color->r) && valid(color->g) && valid(color->b) && valid(color->a)) return {};
        }
        break;
    case PreviewParameterKind::Choice:
        if (const auto* choice = std::get_if<std::string>(&value)) {
            if (std::ranges::find(parameter.choices, *choice) != parameter.choices.end()) return {};
        }
        break;
    }
    return "Invalid value for parameter '" + parameter.id + "'";
}
