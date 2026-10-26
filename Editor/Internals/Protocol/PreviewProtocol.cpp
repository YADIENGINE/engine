#include "Editor/Internals/Protocol/PreviewProtocol.h"
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

nlohmann::json EncodePreviewInspection(const PreviewInspection& inspection) {
    nlohmann::json result{{"values", nlohmann::json::array()},
                          {"tables", nlohmann::json::array()},
                          {"hidden", inspection.hiddenParameters},
                          {"disabled", inspection.disabledParameters},
                          {"disabledActions", inspection.disabledActions},
                          {"showImage", inspection.showImage}};
    if (inspection.focus) {
        const auto& f = *inspection.focus;
        result["focus"] = {f.x, f.y, f.width, f.height};
    }
    for (const auto& value : inspection.values)
        result["values"].push_back({value.label, value.value});
    for (const auto& table : inspection.tables)
        result["tables"].push_back({{"name", table.name}, {"columns", table.columns}, {"rows", table.rows}});
    return result;
}
PreviewInspection DecodePreviewInspection(const nlohmann::json& json) {
    PreviewInspection result;
    if (json.contains("focus")) {
        const auto f = json.at("focus").get<std::vector<int>>();
        if (f.size() != 4 || f[0] < 0 || f[1] < 0 || f[2] <= 0 || f[3] <= 0 || f[0] > 1920 || f[1] > 1080 ||
            f[2] > 1920 - f[0] || f[3] > 1080 - f[1])
            throw std::runtime_error("Invalid preview content bounds");
        result.focus = PreviewContentBounds{f[0], f[1], f[2], f[3]};
    }
    if (json.at("values").size() > 128 || json.at("tables").size() > 16)
        throw std::runtime_error("Preview inspection exceeds capacity");
    for (const auto& value : json.at("values"))
        result.values.push_back({value.at(0).get<std::string>(), value.at(1).get<std::string>()});
    for (const auto& table : json.at("tables")) {
        PreviewTable decoded{table.at("name").get<std::string>(), table.at("columns").get<std::vector<std::string>>(),
                             table.at("rows").get<std::vector<std::vector<std::string>>>()};
        if (decoded.columns.empty() || decoded.columns.size() > 32 || decoded.rows.size() > 1024)
            throw std::runtime_error("Preview table exceeds capacity");
        for (const auto& row : decoded.rows)
            if (row.size() != decoded.columns.size()) throw std::runtime_error("Invalid preview table row");
        result.tables.push_back(std::move(decoded));
    }
    result.hiddenParameters = json.at("hidden").get<std::vector<std::string>>();
    result.disabledParameters = json.at("disabled").get<std::vector<std::string>>();
    result.disabledActions = json.at("disabledActions").get<std::vector<std::string>>();
    if (result.hiddenParameters.size() > 256 || result.disabledParameters.size() > 256 ||
        result.disabledActions.size() > 256)
        throw std::runtime_error("Preview control state exceeds capacity");
    result.showImage = json.at("showImage").get<bool>();
    return result;
}

namespace {
std::uint64_t Unsigned(const nlohmann::json& json) {
    if (!json.is_number_integer() || (!json.is_number_unsigned() && json.get<std::int64_t>() < 0))
        throw std::runtime_error("Expected unsigned protocol integer");
    return json.get<std::uint64_t>();
}
nlohmann::json Point(const std::optional<Vec2>& point) {
    if (!point) return nullptr;
    return nlohmann::json::array({point->x, point->y});
}
std::optional<Vec2> ReadPoint(const nlohmann::json& json) {
    if (json.is_null()) return std::nullopt;
    if (!json.is_array() || json.size() != 2 || !json[0].is_number_integer() || !json[1].is_number_integer())
        throw std::runtime_error("Invalid input point");
    constexpr std::int64_t MaxCoordinate = 1 << 20;
    for (const auto& coordinate : json)
        if (coordinate.is_number_unsigned() &&
            coordinate.get<std::uint64_t>() > static_cast<std::uint64_t>(MaxCoordinate))
            throw std::runtime_error("Input coordinate exceeds capacity");
    const auto x = json[0].get<std::int64_t>();
    const auto y = json[1].get<std::int64_t>();
    if (x < -MaxCoordinate || x > MaxCoordinate || y < -MaxCoordinate || y > MaxCoordinate)
        throw std::runtime_error("Input coordinate exceeds capacity");
    return Vec2{static_cast<int>(x), static_cast<int>(y)};
}
void ReadPoints(const nlohmann::json& json, std::vector<Vec2>& result) {
    if (!json.is_array() || json.size() > 32) throw std::runtime_error("Input point capacity exceeded");
    for (const auto& point : json) {
        const auto value = ReadPoint(point);
        if (!value) throw std::runtime_error("Null input point");
        result.push_back(*value);
    }
}
} // namespace

std::vector<std::byte> EncodePreviewMessage(const PreviewMessage& message) {
    const auto text =
        nlohmann::json{
            {"version", PreviewProtocolVersion}, {"session", message.session}, {"generation", message.generation},
            {"request", message.request},        {"type", message.type},       {"data", message.data}}
            .dump();
    if (text.empty() || text.size() > PreviewMaxMessageBytes)
        throw std::runtime_error("Protocol message exceeds capacity");
    std::vector<std::byte> result;
    result.reserve(text.size() + 4);
    const auto size = static_cast<std::uint32_t>(text.size());
    for (unsigned int shift = 0; shift < 32; shift += 8)
        result.push_back(static_cast<std::byte>((size >> shift) & 0xff));
    for (unsigned char character : text)
        result.push_back(static_cast<std::byte>(character));
    return result;
}
PreviewMessage DecodePreviewMessage(const nlohmann::json& json, const std::string& expectedSession) {
    if (Unsigned(json.at("version")) != PreviewProtocolVersion)
        throw std::runtime_error("Preview protocol version mismatch");
    PreviewMessage result;
    result.session = json.at("session").get<std::string>();
    if (result.session != expectedSession) throw std::runtime_error("Preview session mismatch");
    result.generation = Unsigned(json.at("generation"));
    result.request = Unsigned(json.at("request"));
    result.type = json.at("type").get<std::string>();
    if (result.type.empty() || result.type.size() > 64) throw std::runtime_error("Invalid protocol message type");
    result.data = json.at("data");
    if (!result.data.is_object()) throw std::runtime_error("Invalid protocol message data");
    return result;
}
nlohmann::json EncodePreviewValue(const PreviewValue& value) {
    nlohmann::json data;
    std::visit(
        [&data](const auto& item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, PreviewColor>)
                data = nlohmann::json::array({item.r, item.g, item.b, item.a});
            else
                data = item;
        },
        value);
    return {{"type", value.index()}, {"value", data}};
}
PreviewValue DecodePreviewValue(const nlohmann::json& json) {
    const auto& value = json.at("value");
    switch (Unsigned(json.at("type"))) {
    case 0:
        if (!value.is_number_integer() ||
            (value.is_number_unsigned() &&
             value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())))
            break;
        return value.get<std::int64_t>();
    case 1:
        if (value.is_number() && std::isfinite(value.get<double>())) return value.get<double>();
        break;
    case 2:
        if (value.is_boolean()) return value.get<bool>();
        break;
    case 3:
        if (value.is_array() && value.size() == 4) {
            for (const auto& channel : value)
                if (!channel.is_number() || !std::isfinite(channel.get<double>()) || channel.get<double>() < 0.0 ||
                    channel.get<double>() > 1.0)
                    throw std::runtime_error("Invalid color channel");
            return PreviewColor{value[0].get<float>(), value[1].get<float>(), value[2].get<float>(),
                                value[3].get<float>()};
        }
        break;
    case 4:
        if (value.is_string()) return value.get<std::string>();
        break;
    }
    throw std::runtime_error("Invalid preview parameter value");
}
nlohmann::json EncodePreviewParameters(const PreviewParameters& parameters) {
    auto result = nlohmann::json::object();
    for (const auto& [id, value] : parameters)
        result[id] = EncodePreviewValue(value);
    return result;
}
PreviewParameters DecodePreviewParameters(const nlohmann::json& json) {
    if (!json.is_object() || json.size() > 128) throw std::runtime_error("Invalid parameter collection");
    PreviewParameters result;
    for (const auto& [id, value] : json.items())
        result.emplace(id, DecodePreviewValue(value));
    return result;
}
nlohmann::json EncodePreviewCatalog(const std::vector<PreviewDefinition>& definitions) {
    auto entries = nlohmann::json::array();
    for (const auto& definition : definitions) {
        auto parameters = nlohmann::json::array();
        for (const auto& parameter : definition.parameters)
            parameters.push_back({{"id", parameter.id},
                                  {"label", parameter.label},
                                  {"kind", parameter.kind},
                                  {"initial", EncodePreviewValue(parameter.initial)},
                                  {"minimum", parameter.minimum},
                                  {"maximum", parameter.maximum},
                                  {"apply", parameter.apply},
                                  {"choices", parameter.choices},
                                  {"group", parameter.group},
                                  {"description", parameter.description},
                                  {"choiceButtons", parameter.choiceButtons},
                                  {"choiceButtonColumns", parameter.choiceButtonColumns},
                                  {"numericInput", parameter.numericInput}});
        auto actions = nlohmann::json::array();
        for (const auto& action : definition.actions)
            actions.push_back({{"id", action.id},
                               {"label", action.label},
                               {"description", action.description},
                               {"primary", action.primary}});
        entries.push_back({{"id", definition.id},
                           {"name", definition.name},
                           {"category", definition.category},
                           {"description", definition.description},
                           {"parameters", parameters},
                           {"actions", actions}});
    }
    return {{"entries", entries}};
}
std::vector<PreviewDefinition> DecodePreviewCatalog(const nlohmann::json& json) {
    const auto& entries = json.at("entries");
    if (!entries.is_array() || entries.size() > 2048) throw std::runtime_error("Catalog exceeds capacity");
    std::vector<PreviewDefinition> result;
    std::set<std::string> identifiers;
    for (const auto& entry : entries) {
        PreviewDefinition definition;
        definition.id = entry.at("id").get<std::string>();
        definition.name = entry.at("name").get<std::string>();
        definition.category = entry.at("category").get<std::string>();
        definition.description = entry.at("description").get<std::string>();
        if (definition.id.empty() || definition.name.empty() || !identifiers.insert(definition.id).second)
            throw std::runtime_error("Invalid catalog identifier");
        std::set<std::string> parameterIds;
        if (!entry.at("parameters").is_array() || entry.at("parameters").size() > 128)
            throw std::runtime_error("Invalid parameter list/capacity");
        for (const auto& item : entry.at("parameters")) {
            PreviewParameter parameter;
            parameter.id = item.at("id").get<std::string>();
            parameter.label = item.at("label").get<std::string>();
            const auto kind = Unsigned(item.at("kind"));
            const auto apply = Unsigned(item.at("apply"));
            if (kind > static_cast<std::uint64_t>(PreviewParameterKind::Choice) ||
                apply > static_cast<std::uint64_t>(PreviewApplyMode::OnReset))
                throw std::runtime_error("Unsupported parameter metadata");
            parameter.kind = static_cast<PreviewParameterKind>(kind);
            parameter.apply = static_cast<PreviewApplyMode>(apply);
            parameter.initial = DecodePreviewValue(item.at("initial"));
            parameter.minimum = item.at("minimum").get<double>();
            parameter.maximum = item.at("maximum").get<double>();
            parameter.choices = item.at("choices").get<std::vector<std::string>>();
            parameter.group = item.value("group", std::string{});
            parameter.description = item.value("description", std::string{});
            parameter.choiceButtons = item.value("choiceButtons", false);
            parameter.choiceButtonColumns = item.value("choiceButtonColumns", 0);
            parameter.numericInput = item.value("numericInput", false);
            if (!parameterIds.insert(parameter.id).second || !ValidatePreviewParameter(parameter).empty())
                throw std::runtime_error("Invalid parameter metadata");
            definition.parameters.push_back(std::move(parameter));
        }
        std::set<std::string> actionIds;
        bool hasPrimaryAction = false;
        if (!entry.at("actions").is_array() || entry.at("actions").size() > 128)
            throw std::runtime_error("Invalid action list/capacity");
        for (const auto& item : entry.at("actions")) {
            PreviewAction action{item.at("id").get<std::string>(), item.at("label").get<std::string>()};
            action.description = item.value("description", std::string{});
            action.primary = item.value("primary", false);
            if (action.id.empty() || !actionIds.insert(action.id).second)
                throw std::runtime_error("Invalid action metadata");
            if (action.primary && hasPrimaryAction)
                throw std::runtime_error("Multiple primary actions in preview metadata");
            hasPrimaryAction = hasPrimaryAction || action.primary;
            definition.actions.push_back(std::move(action));
        }
        result.push_back(std::move(definition));
    }
    return result;
}
nlohmann::json EncodePreviewInput(const DeviceInput& input) {
    auto taps = nlohmann::json::array();
    auto held = nlohmann::json::array();
    for (Vec2 point : input.taps)
        taps.push_back(Point(point));
    for (Vec2 point : input.held)
        held.push_back(Point(point));
    return {{"pressed", input.keysPressed},
            {"down", input.keysDown},
            {"shift", input.shiftHeld},
            {"mouse", Point(input.mouse)},
            {"pointer", Point(input.pointer)},
            {"hover", Point(input.hover)},
            {"mousePressed", input.mousePressed},
            {"mouseReleased", input.mouseReleased},
            {"mouseDown", input.mouseDown},
            {"wheel", input.wheel},
            {"taps", taps},
            {"held", held}};
}
DeviceInput DecodePreviewInput(const nlohmann::json& json) {
    DeviceInput input;
    const auto& pressed = json.at("pressed");
    const auto& down = json.at("down");
    if (!pressed.is_array() || !down.is_array() || pressed.size() != input.keysPressed.size() ||
        down.size() != input.keysDown.size())
        throw std::runtime_error("Input key layout mismatch");
    for (std::size_t i = 0; i < input.keysDown.size(); ++i) {
        input.keysPressed[i] = pressed[i].get<bool>();
        input.keysDown[i] = down[i].get<bool>();
    }
    input.shiftHeld = json.at("shift").get<bool>();
    input.mouse = ReadPoint(json.at("mouse"));
    input.pointer = ReadPoint(json.at("pointer"));
    input.hover = ReadPoint(json.at("hover"));
    input.mousePressed = json.at("mousePressed").get<bool>();
    input.mouseReleased = json.at("mouseReleased").get<bool>();
    input.mouseDown = json.at("mouseDown").get<bool>();
    input.wheel = json.at("wheel").get<float>();
    if (!std::isfinite(input.wheel)) throw std::runtime_error("Invalid input wheel");
    ReadPoints(json.at("taps"), input.taps);
    ReadPoints(json.at("held"), input.held);
    return input;
}
