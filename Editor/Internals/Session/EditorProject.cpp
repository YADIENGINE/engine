#include "Editor/Internals/Session/EditorProject.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace {
std::filesystem::path Resolve(const std::filesystem::path& root, const std::string& text) {
    const std::filesystem::path path(std::u8string(text.begin(), text.end()));
    return path.is_absolute() ? path : std::filesystem::absolute(root / path).lexically_normal();
}
ProcessCommand ReadCommand(const nlohmann::json& json, const std::filesystem::path& root) {
    ProcessCommand command;
    command.executable = Resolve(root, json.at("executable").get<std::string>());
    command.directory = Resolve(root, json.value("directory", "."));
    command.arguments = json.value("arguments", std::vector<std::string>{});
    command.environment = json.value("environment", std::map<std::string, std::string>{});
    if (json.at("executable").get<std::string>().empty())
        throw std::runtime_error("Missing executable in editor project");
    for (const auto& argument : command.arguments)
        if (argument.find('\0') != std::string::npos) throw std::runtime_error("NUL in process argument");
    return command;
}
} // namespace

EditorProject ReadEditorProject(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open editor project: " + file.string());
    const auto json = nlohmann::json::parse(input);
    if (json.at("version").get<int>() != 1) throw std::runtime_error("Unsupported editor project version");
    const auto root = std::filesystem::absolute(file).parent_path();
    EditorProject project;
    project.name = json.value("name", "Scene project");
    project.worker = ReadCommand(json.at("worker"), root);
    project.build = ReadCommand(json.at("build"), root);
    project.buildDirectory = Resolve(root, json.at("buildDirectory").get<std::string>());
    if (const auto source = json.value("sourceDirectory", std::string{}); !source.empty())
        project.sourceDirectory = Resolve(root, source);
    if (const auto engine = json.value("engineDirectory", std::string{}); !engine.empty())
        project.engineDirectory = Resolve(root, engine);
    if (const auto assets = json.value("assetsDirectory", std::string{}); !assets.empty())
        project.assetsDirectory = Resolve(root, assets);
    if (const auto python = json.value("python", std::string{}); !python.empty())
        project.python = Resolve(root, python);
    return project;
}
