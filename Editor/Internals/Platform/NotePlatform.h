#pragma once
#include <filesystem>
#include <string>

void RenameNote(const std::filesystem::path& from, const std::filesystem::path& to);
void RecycleNote(const std::filesystem::path& path);
