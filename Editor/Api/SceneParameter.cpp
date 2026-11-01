#include "Editor/Api/SceneParameter.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

std::string ValidateSceneParameter(const SceneParameter& parameter) {
    if (parameter.id.empty() || !std::isfinite(parameter.minimum) || !std::isfinite(parameter.maximum) ||
        parameter.minimum > parameter.maximum || parameter.choiceButtonColumns < 0 ||
        parameter.choiceButtonColumns > 64 ||
        (!parameter.applyAction.empty() && parameter.apply != SceneApplyMode::NextAction))
        return "Invalid parameter metadata: " + parameter.id;
    if (parameter.kind == SceneParameterKind::Integer) {
        const double lowest = static_cast<double>(std::numeric_limits<std::int64_t>::min());
        if (parameter.minimum < lowest || parameter.maximum >= -lowest ||
            std::floor(parameter.minimum) != parameter.minimum || std::floor(parameter.maximum) != parameter.maximum)
            return "Integer range is not representable: " + parameter.id;
    }
    if (parameter.kind == SceneParameterKind::Choice) {
        std::set<std::string> choices;
        for (const auto& choice : parameter.choices)
            if (choice.empty() || !choices.insert(choice).second) return "Invalid parameter choices: " + parameter.id;
    }
    return ValidateSceneValue(parameter, parameter.initial);
}

std::string ValidateSceneValue(const SceneParameter& parameter, const SceneValue& value) {
    switch (parameter.kind) {
    case SceneParameterKind::Integer:
        if (const auto* number = std::get_if<std::int64_t>(&value)) {
            if (static_cast<double>(*number) >= parameter.minimum && static_cast<double>(*number) <= parameter.maximum)
                return {};
        }
        break;
    case SceneParameterKind::Float:
        if (const auto* number = std::get_if<double>(&value)) {
            if (std::isfinite(*number) && *number >= parameter.minimum && *number <= parameter.maximum) return {};
        }
        break;
    case SceneParameterKind::Boolean:
        if (std::holds_alternative<bool>(value)) return {};
        break;
    case SceneParameterKind::Color:
        if (const auto* color = std::get_if<SceneColor>(&value)) {
            const auto valid = [](float channel) {
                return std::isfinite(channel) && channel >= 0.0f && channel <= 1.0f;
            };
            if (valid(color->r) && valid(color->g) && valid(color->b) && valid(color->a)) return {};
        }
        break;
    case SceneParameterKind::Choice:
        if (const auto* choice = std::get_if<std::string>(&value)) {
            if (std::ranges::find(parameter.choices, *choice) != parameter.choices.end()) return {};
        }
        break;
    }
    return "Invalid value for parameter '" + parameter.id + "'";
}
