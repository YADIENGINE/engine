#include "Editor/Internals/Session/AgentContext.h"
#include <algorithm>
#include <format>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
const char* LevelName(EditorLogLevel level) {
    switch (level) {
    case EditorLogLevel::Info   : return "info";
    case EditorLogLevel::Warning: return "warning";
    case EditorLogLevel::Error  : return "error";
    }
    return "info";
}
} // namespace

std::string ChangedParameters(const SceneDefinition& definition, const SceneParameters& values) {
    std::string changed;
    for (const auto& parameter : definition.parameters) {
        const auto value = values.find(parameter.id);
        if (value == values.end() || value->second == parameter.initial) continue;
        changed += " " + parameter.id + "=" + ControlValue(value->second).dump();
    }
    return changed;
}
std::string SceneContext(const SceneDefinition& definition, const SceneParameters& values, double seconds, bool paused,
                         Vec2 canvas, const std::filesystem::path& image) {
    std::string line =
        std::format("[editor] scene={} t{}{:.2f}s({}) {}x{} image=\"{}\"", definition.id, paused ? "=" : "~", seconds,
                    paused ? "paused" : "playing", canvas.x, canvas.y, Utf8(image));
    if (const auto changed = ChangedParameters(definition, values); !changed.empty()) line += " params:" + changed;
    return line + " ";
}
std::string ParameterContext(const SceneDefinition& definition, const SceneParameter& parameter,
                             const SceneValue& value) {
    std::string line = std::format("[editor] scene={} parameter={} \"{}\" = {}", definition.id, parameter.id,
                                   parameter.label, ControlValue(value).dump());
    if (parameter.kind == SceneParameterKind::Choice) {
        std::string choices;
        for (const auto& choice : parameter.choices)
            choices += (choices.empty() ? "" : "|") + choice;
        line += " choices: " + choices;
    } else if (parameter.kind == SceneParameterKind::Integer || parameter.kind == SceneParameterKind::Float) {
        line += std::format(" range: {}..{}", parameter.minimum, parameter.maximum);
    }
    if (ControlValue(value) != ControlValue(parameter.initial))
        line += " initial: " + ControlValue(parameter.initial).dump();
    if (!parameter.description.empty()) line += " - " + parameter.description;
    return line + " ";
}
std::string OutputContext(const std::vector<EditorLogEntry>& entries, std::size_t limit) {
    const auto shown = std::min(entries.size(), limit);
    std::string text = std::format("[editor] output, {} line{}", entries.size(), entries.size() == 1 ? "" : "s");
    if (shown < entries.size()) text += std::format(" (newest {})", shown);
    text += ":\n";
    for (auto entry = entries.end() - static_cast<std::ptrdiff_t>(shown); entry != entries.end(); ++entry)
        text += std::format("{}: {}\n", LevelName(entry->level), entry->text);
    return text;
}
std::string BuildFailureContext(const EditorBuildAttempt& attempt, const std::deque<EditorLogEntry>& attemptEntries,
                                std::size_t limit) {
    if (attempt.result != EditorBuildResult::BuildFailed && attempt.result != EditorBuildResult::RefreshFailed)
        return {};
    std::string text = attempt.result == EditorBuildResult::RefreshFailed ? "[editor] Build & Refresh: build "
                                                                            "succeeded, refresh failed"
                                                                          : "[editor] Build & Refresh failed";
    if (attempt.exitCode) text += std::format(" (exit code {})", *attempt.exitCode);
    std::vector<const EditorLogEntry*> errors;
    for (const auto& entry : attemptEntries)
        if (entry.level == EditorLogLevel::Error &&
            std::ranges::none_of(errors, [&](const EditorLogEntry* seen) { return seen->text == entry.text; }))
            errors.push_back(&entry);
    if (errors.empty() && attempt.firstError) errors.push_back(&*attempt.firstError);
    text += std::format(", {} error line{}", errors.size(), errors.size() == 1 ? "" : "s");
    if (errors.size() > limit) text += std::format(" (first {})", limit);
    text += ":\n";
    for (std::size_t i = 0; i < std::min(errors.size(), limit); ++i)
        text += errors[i]->text + "\n";
    return text;
}
