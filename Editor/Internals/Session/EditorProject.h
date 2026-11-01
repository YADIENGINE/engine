#pragma once
#include <filesystem>
#include "Editor/Internals/Process/ProcessCommand.h"

struct EditorProject {
    std::string name;
    ProcessCommand worker;
    ProcessCommand build;
    std::filesystem::path buildDirectory;
    std::filesystem::path sourceDirectory; // proje kökü (konsolun çalışma klasörü); eski tariflerde boş
    std::filesystem::path engineDirectory; // Asset library roots; empty in older recipes
    std::filesystem::path assetsDirectory;
    std::filesystem::path python; // Interpreter for engine tools; empty: the Assets panel's file actions are off
};

[[nodiscard]] EditorProject ReadEditorProject(const std::filesystem::path& file);
