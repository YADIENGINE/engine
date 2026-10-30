#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
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
std::string Line(const nlohmann::json& json) {
    return json.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
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
EditorControlCommand DecodeControlRequest(std::string_view line) {
    const auto request = nlohmann::json::parse(line);
    if (!request.is_object() || request.value("version", 0) != EditorControlVersion)
        throw std::runtime_error("Unsupported editor control version");
    const auto command = request.at("command").get<std::string>();
    if (command == "refresh") return EditorControlCommand::Refresh;
    throw std::runtime_error("Unknown editor control command: " + command);
}
std::string EncodeControlLog(const EditorLogEntry& entry) {
    return Line({{"type", "log"},
                 {"level", LevelName(entry.level)},
                 {"source", SourceName(entry.source)},
                 {"text", entry.text}});
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
std::string EncodeControlError(std::string_view text) { return Line({{"type", "error"}, {"text", std::string(text)}}); }
