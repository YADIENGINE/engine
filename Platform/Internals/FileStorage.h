#pragma once
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>

[[nodiscard]] std::string ReadBinaryFile(const std::filesystem::path& file,
                                         std::size_t maxBytes = std::numeric_limits<std::size_t>::max());
// OwnerOnly: POSIX'te dosya ve klasör yalnız sahibine açık (Windows'ta içerik ayrıca korunmalı, ör. DPAPI).
enum class FileAccess { Shared, OwnerOnly };
// CreateNew: hedef zaten varsa dokunmaz; yarışta kaybeden yazıcı false alır.
enum class WriteMode { Replace, CreateNew };

// Returns false only for WriteMode::CreateNew when another writer already created the destination.
bool WriteBinaryFileAtomic(const std::filesystem::path& file, std::string_view bytes,
                           FileAccess access = FileAccess::Shared, WriteMode mode = WriteMode::Replace);
// Preserves the source under a unique .invalid name; never replaces an existing archive.
[[nodiscard]] std::filesystem::path ArchiveInvalidFile(const std::filesystem::path& file);
