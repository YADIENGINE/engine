#pragma once
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>

[[nodiscard]] std::string ReadBinaryFile(const std::filesystem::path& file,
                                         std::size_t maxBytes = std::numeric_limits<std::size_t>::max());
// Returns false only when replace=false and another writer already created the destination.
bool WriteBinaryFileAtomic(const std::filesystem::path& file, std::string_view bytes, bool privateFile = false,
                           bool replace = true);
// Preserves the source under a unique .invalid name; never replaces an existing archive.
[[nodiscard]] std::filesystem::path ArchiveInvalidFile(const std::filesystem::path& file);
