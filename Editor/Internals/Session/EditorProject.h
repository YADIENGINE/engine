#pragma once
#include <filesystem>
#include "Editor/Internals/Process/ProcessCommand.h"

struct EditorProject {
    std::string name;
    ProcessCommand worker;
    ProcessCommand build;
    std::filesystem::path buildDirectory;
    std::filesystem::path sourceDirectory; // proje kökü (konsolun çalışma klasörü); eski tariflerde boş
};

[[nodiscard]] EditorProject ReadEditorProject(const std::filesystem::path& file);
