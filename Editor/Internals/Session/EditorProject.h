#pragma once
#include <filesystem>
#include "Editor/Internals/Process/ProcessCommand.h"

struct EditorProject {
    std::string name;
    ProcessCommand worker;
    ProcessCommand build;
    std::filesystem::path buildDirectory;
};

[[nodiscard]] EditorProject ReadEditorProject(const std::filesystem::path& file);
