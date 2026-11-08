#include "Editor/Internals/Platform/NotePlatform.h"
#include <stdexcept>

void RenameNote(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (std::filesystem::exists(to)) throw std::runtime_error("That name already exists");
    std::filesystem::rename(from, to);
}
void RecycleNote(const std::filesystem::path&) { throw std::runtime_error("The editor Recycle Bin requires Windows"); }
