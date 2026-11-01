#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include "Utilities/EnumTable.h"

namespace {
struct CommandRow {
    EditorControlCommand id;
    std::string_view name;
};
constexpr std::array CommandRows{
    CommandRow{EditorControlCommand::Refresh, "refresh"},
    CommandRow{EditorControlCommand::Status, "status"},
    CommandRow{EditorControlCommand::List, "list"},
    CommandRow{EditorControlCommand::Open, "open"},
    CommandRow{EditorControlCommand::Set, "set"},
    CommandRow{EditorControlCommand::Action, "action"},
    CommandRow{EditorControlCommand::Pause, "pause"},
    CommandRow{EditorControlCommand::Play, "play"},
    CommandRow{EditorControlCommand::Step, "step"},
    CommandRow{EditorControlCommand::Speed, "speed"},
    CommandRow{EditorControlCommand::Reset, "reset"},
    CommandRow{EditorControlCommand::Seek, "seek"},
    CommandRow{EditorControlCommand::Screenshot, "screenshot"},
    CommandRow{EditorControlCommand::Frames, "frames"},
    CommandRow{EditorControlCommand::Inspect, "inspect"},
    CommandRow{EditorControlCommand::Logs, "logs"},
    CommandRow{EditorControlCommand::Note, "note"},
    CommandRow{EditorControlCommand::Audio, "audio"},
    CommandRow{EditorControlCommand::AudioControl, "audio-control"},
};
static_assert(RowsInIdOrder(CommandRows));

const char* LevelName(EditorLogLevel level) {
    switch (level) {
    case EditorLogLevel::Info   : return "info";
    case EditorLogLevel::Warning: return "warning";
    case EditorLogLevel::Error  : return "error";
    }
    return "info";
}
const char* SourceName(EditorLogSource source) {
    switch (source) {
    case EditorLogSource::Editor: return "editor";
    case EditorLogSource::Worker: return "worker";
    case EditorLogSource::Build : return "build";
    }
    return "editor";
}
const char* KindName(SceneParameterKind kind) {
    switch (kind) {
    case SceneParameterKind::Integer: return "integer";
    case SceneParameterKind::Float  : return "float";
    case SceneParameterKind::Boolean: return "boolean";
    case SceneParameterKind::Color  : return "color";
    case SceneParameterKind::Choice : return "choice";
    }
    return "float";
}
const char* ApplyName(SceneApplyMode apply) {
    switch (apply) {
    case SceneApplyMode::Immediate : return "immediate";
    case SceneApplyMode::NextAction: return "next-action";
    case SceneApplyMode::OnReset   : return "on-reset";
    }
    return "immediate";
}
std::string Line(const nlohmann::json& json) {
    return json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}
[[noreturn]] void Mismatch(const SceneParameter& parameter, std::string_view expected) {
    throw std::runtime_error("Parameter '" + parameter.id + "' expects " + std::string(expected));
}
float HexChannel(std::string_view text, std::size_t at) {
    unsigned value = 0;
    const auto [end, error] = std::from_chars(text.data() + at, text.data() + at + 2, value, 16);
    if (error != std::errc{} || end != text.data() + at + 2)
        throw std::runtime_error("Invalid color: " + std::string(text));
    return static_cast<float>(value) / 255.0f;
}
SceneColor ColorOf(const SceneParameter& parameter, const nlohmann::json& value) {
    if (value.is_string()) {
        const auto text = value.get<std::string>();
        if (!text.starts_with('#') || (text.size() != 7 && text.size() != 9)) Mismatch(parameter, "\"#rrggbb(aa)\"");
        return {HexChannel(text, 1), HexChannel(text, 3), HexChannel(text, 5),
                text.size() == 9 ? HexChannel(text, 7) : 1.0f};
    }
    if (!value.is_array() || (value.size() != 3 && value.size() != 4)) Mismatch(parameter, "[r, g, b(, a)]");
    std::array<float, 4> channels{1.0f, 1.0f, 1.0f, 1.0f};
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (!value[i].is_number()) Mismatch(parameter, "[r, g, b(, a)] numbers");
        channels[i] = value[i].get<float>();
    }
    return {channels[0], channels[1], channels[2], channels[3]};
}
} // namespace

std::string EncodeControlAnnouncement(const EditorControlAnnouncement& announcement) {
    return Line({{"version", EditorControlVersion}, {"pipe", announcement.pipe}, {"pid", announcement.processId}});
}
EditorControlAnnouncement DecodeControlAnnouncement(std::string_view json) {
    const auto data = nlohmann::json::parse(json);
    if (data.at("version").get<int>() != EditorControlVersion)
        throw std::runtime_error("Unsupported editor control version");
    return {data.at("pipe").get<std::string>(), data.at("pid").get<unsigned long>()};
}
EditorControlRequest DecodeControlRequest(std::string_view line) {
    auto request = nlohmann::json::parse(line);
    if (!request.is_object() || request.value("version", 0) != EditorControlVersion)
        throw std::runtime_error("Unsupported editor control version (this editor speaks " +
                                 std::to_string(EditorControlVersion) + ")");
    const auto name = request.at("command").get<std::string>();
    for (const auto& row : CommandRows) {
        if (row.name != name) continue;
        request.erase("version");
        request.erase("command");
        return {row.id, std::move(request)};
    }
    throw std::runtime_error("Unknown editor control command: " + name);
}
std::string_view ControlCommandName(EditorControlCommand command) {
    return CommandRows.at(static_cast<std::size_t>(command)).name;
}
const char* AgentActivityName(AgentActivityState state) {
    switch (state) {
    case AgentActivityState::Idle   : return "idle";
    case AgentActivityState::Working: return "working";
    case AgentActivityState::Waiting: return "waiting";
    }
    return "idle";
}
AgentNote DecodeAgentNote(const nlohmann::json& arguments) {
    AgentNote note;
    if (const auto text = arguments.find("text"); text != arguments.end()) {
        if (!text->is_string()) throw std::runtime_error("\"text\" must be a string");
        note.text = text->get<std::string>().substr(0, AgentNoteMaxBytes);
    }
    if (const auto session = arguments.find("session"); session != arguments.end() && session->is_string())
        note.session = session->get<std::string>().substr(0, AgentSessionMaxBytes);
    if (const auto state = arguments.find("state"); state != arguments.end()) {
        const auto name = state->is_string() ? state->get<std::string>() : std::string{};
        bool known = false;
        for (const auto candidate :
             {AgentActivityState::Idle, AgentActivityState::Working, AgentActivityState::Waiting})
            if (name == AgentActivityName(candidate)) {
                note.state = candidate;
                known = true;
            }
        if (!known) throw std::runtime_error("\"state\" must be working, waiting or idle");
    }
    return note;
}
bool ControlCommandChangesEditor(EditorControlCommand command) {
    switch (command) {
    case EditorControlCommand::Refresh:
    case EditorControlCommand::Open:
    case EditorControlCommand::Set:
    case EditorControlCommand::Action:
    case EditorControlCommand::Pause:
    case EditorControlCommand::Play:
    case EditorControlCommand::Step:
    case EditorControlCommand::Speed:
    case EditorControlCommand::Reset:
    case EditorControlCommand::Seek:
    case EditorControlCommand::Frames:
    case EditorControlCommand::AudioControl: return true;
    case EditorControlCommand::Status      :
    case EditorControlCommand::List        :
    case EditorControlCommand::Screenshot  :
    case EditorControlCommand::Inspect     :
    case EditorControlCommand::Logs        :
    case EditorControlCommand::Note        :
    case EditorControlCommand::Audio       :
    case EditorControlCommand::Count       : return false;
    }
    return false;
}
nlohmann::json ControlLogEntry(const EditorLogEntry& entry) {
    return {{"sequence", entry.sequence},
            {"level", LevelName(entry.level)},
            {"source", SourceName(entry.source)},
            {"text", entry.text}};
}
std::string EncodeControlLog(const EditorLogEntry& entry) {
    auto line = ControlLogEntry(entry);
    line["type"] = "log";
    return Line(line);
}
std::string EncodeControlResult(EditorBuildResult result) {
    const char* name = nullptr;
    switch (result) {
    case EditorBuildResult::Succeeded    : name = "succeeded"; break;
    case EditorBuildResult::BuildFailed  : name = "build-failed"; break;
    case EditorBuildResult::RefreshFailed: name = "refresh-failed"; break;
    case EditorBuildResult::Canceled     : name = "canceled"; break;
    case EditorBuildResult::None         :
    case EditorBuildResult::Running      : throw std::logic_error("The attempt has not finished");
    }
    return Line({{"type", "result"}, {"result", name}});
}
std::string EncodeControlReply(nlohmann::json data) {
    data["type"] = "result";
    return Line(data);
}
std::string EncodeControlError(std::string_view text) { return Line({{"type", "error"}, {"text", std::string(text)}}); }

nlohmann::json ControlValue(const SceneValue& value) {
    nlohmann::json data;
    std::visit(
        [&data](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, SceneColor>)
                data = nlohmann::json::array({item.r, item.g, item.b, item.a});
            else
                data = item;
        },
        value);
    return data;
}
SceneValue ControlValueOf(const SceneParameter& parameter, const nlohmann::json& value) {
    switch (parameter.kind) {
    case SceneParameterKind::Integer: {
        if (value.is_number_integer()) return value.get<std::int64_t>();
        if (value.is_number_float()) {
            const double number = value.get<double>();
            if (std::isfinite(number) && std::trunc(number) == number) return static_cast<std::int64_t>(number);
        }
        Mismatch(parameter, "a whole number");
    }
    case SceneParameterKind::Float:
        if (!value.is_number()) Mismatch(parameter, "a number");
        return value.get<double>();
    case SceneParameterKind::Boolean:
        if (!value.is_boolean()) Mismatch(parameter, "true or false");
        return value.get<bool>();
    case SceneParameterKind::Color: return ColorOf(parameter, value);
    case SceneParameterKind::Choice:
        if (!value.is_string()) Mismatch(parameter, "one of its choices");
        return value.get<std::string>();
    }
    Mismatch(parameter, "a known kind");
}
nlohmann::json ControlCatalogEntry(const SceneDefinition& definition, const SceneParameters& values) {
    auto parameters = nlohmann::json::array();
    for (const auto& parameter : definition.parameters) {
        nlohmann::json entry{{"id", parameter.id},
                             {"label", parameter.label},
                             {"kind", KindName(parameter.kind)},
                             {"apply", ApplyName(parameter.apply)}};
        const auto current = values.find(parameter.id);
        entry["value"] = ControlValue(current == values.end() ? parameter.initial : current->second);
        if (parameter.kind == SceneParameterKind::Integer || parameter.kind == SceneParameterKind::Float) {
            entry["minimum"] = parameter.minimum;
            entry["maximum"] = parameter.maximum;
        }
        if (parameter.kind == SceneParameterKind::Choice) entry["choices"] = parameter.choices;
        if (!parameter.group.empty()) entry["group"] = parameter.group;
        if (!parameter.description.empty()) entry["description"] = parameter.description;
        if (!parameter.applyAction.empty()) entry["applyAction"] = parameter.applyAction;
        parameters.push_back(std::move(entry));
    }
    auto actions = nlohmann::json::array();
    for (const auto& action : definition.actions) {
        nlohmann::json entry{{"id", action.id}, {"label", action.label}};
        if (!action.description.empty()) entry["description"] = action.description;
        if (action.primary) entry["primary"] = true;
        actions.push_back(std::move(entry));
    }
    nlohmann::json entry{{"id", definition.id}, {"name", definition.name}, {"category", definition.category}};
    if (!definition.description.empty()) entry["description"] = definition.description;
    if (definition.asset) {
        entry["kind"] = "graphic";
        entry["file"] = definition.asset->file;
        if (!definition.asset->usage.empty()) entry["usage"] = definition.asset->usage;
    }
    entry["parameters"] = std::move(parameters);
    entry["actions"] = std::move(actions);
    return entry;
}
