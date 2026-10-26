#pragma once
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include "Editor/Internals/Settings/EditorPreferences.h"

class EditorSettingsStore {
public:
    explicit EditorSettingsStore(std::filesystem::path file);
    [[nodiscard]] std::string Load(const std::string& project, EditorPreferences& preferences, int& fontSize);
    [[nodiscard]] std::string Save(const std::string& project, const EditorPreferences& preferences, int fontSize);

private:
    std::filesystem::path _file;
    nlohmann::json _document = nlohmann::json::object();
};
