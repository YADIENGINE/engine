#pragma once
#include <filesystem>
#include <span>
#include <string>

[[nodiscard]] std::filesystem::path PrepareAssets(std::span<const std::filesystem::path> files);
[[nodiscard]] std::filesystem::path SaveFilePath(const std::filesystem::path& applicationId,
                                                 const std::filesystem::path& filename);
