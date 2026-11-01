#pragma once
#include <cstdint>
#include <filesystem>

[[nodiscard]] std::int64_t SaveRecipeProgress(const std::filesystem::path& directory, std::int64_t completed);
