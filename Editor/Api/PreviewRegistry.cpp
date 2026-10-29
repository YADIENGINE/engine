#include "Editor/Api/PreviewRegistry.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

PreviewRegistry::PreviewRegistry() = default;
void PreviewRegistry::Add(PreviewDefinition definition) {
    if (definition.id.empty() || definition.name.empty() || !definition.create || Find(definition.id))
        throw std::invalid_argument("Preview registration needs a unique ID, name and factory");
    std::set<std::string> parameters;
    for (const auto& parameter : definition.parameters) {
        if (!parameters.insert(parameter.id).second || !ValidatePreviewParameter(parameter).empty())
            throw std::invalid_argument("Invalid preview parameter: " + parameter.id);
    }
    std::set<std::string> actions;
    if (definition.benchmark) {
        if (definition.benchmark->id.empty() || definition.benchmark->id.size() > 256 ||
            definition.benchmark->version < 1)
            throw std::invalid_argument("Invalid benchmark identity/version");
        std::set<std::string> variants;
        for (const auto& variant : definition.benchmark->variants) {
            if (variant.name.empty() || variant.name.size() > 1024 || !variants.insert(variant.name).second)
                throw std::invalid_argument("Invalid benchmark variant name");
            for (const auto& [id, value] : variant.parameters) {
                const auto parameter = std::ranges::find(definition.parameters, id, &PreviewParameter::id);
                if (parameter == definition.parameters.end() || !ValidatePreviewValue(*parameter, value).empty())
                    throw std::invalid_argument("Invalid benchmark parameter override");
            }
        }
    }
    bool hasPrimaryAction = false;
    for (const auto& action : definition.actions) {
        if (action.id.empty() || !actions.insert(action.id).second)
            throw std::invalid_argument("Invalid preview action: " + action.id);
        if (action.primary && hasPrimaryAction)
            throw std::invalid_argument("Preview actions may designate only one primary action");
        hasPrimaryAction = hasPrimaryAction || action.primary;
    }
    for (const auto& parameter : definition.parameters)
        if (!parameter.applyAction.empty() && !actions.contains(parameter.applyAction))
            throw std::invalid_argument("Unknown parameter apply action: " + parameter.applyAction);
    _entries.push_back(std::move(definition));
}
const PreviewDefinition* PreviewRegistry::Find(const std::string& id) const {
    const auto found = std::ranges::find(_entries, id, &PreviewDefinition::id);
    return found == _entries.end() ? nullptr : &*found;
}
bool MergePreviewCapture(const PreviewDefinition& definition, const PreviewParameters& captured,
                         PreviewParameters& accepted) {
    auto next = accepted;
    for (const auto& [id, value] : captured) {
        const auto parameter = std::ranges::find(definition.parameters, id, &PreviewParameter::id);
        if (parameter == definition.parameters.end() || !ValidatePreviewValue(*parameter, value).empty())
            throw std::invalid_argument("Invalid captured preview parameter: " + id);
        if (parameter->apply != PreviewApplyMode::OnReset) next[id] = value;
    }
    if (next == accepted) return false;
    accepted = std::move(next);
    return true;
}
PreviewParameters PreviewDefaults(const PreviewDefinition& definition) {
    PreviewParameters result;
    for (const auto& parameter : definition.parameters)
        result.emplace(parameter.id, parameter.initial);
    return result;
}
PreviewParameters RestorePreviewParameters(const PreviewDefinition& definition, const PreviewParameters& saved,
                                           std::vector<std::string>& messages) {
    auto result = PreviewDefaults(definition);
    for (const auto& [id, value] : saved) {
        const auto parameter = std::ranges::find(definition.parameters, id, &PreviewParameter::id);
        if (parameter == definition.parameters.end() || !ValidatePreviewValue(*parameter, value).empty())
            messages.push_back("Discarded incompatible parameter: " + id);
        else
            result[id] = value;
    }
    return result;
}
