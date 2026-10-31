#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "Modules/JsonIO/JsonIO.h"

enum class SaveEncryptionMode { Plaintext, Encrypted };
enum class SaveLoadResult { Missing, Loaded, Recovered };
// clang-format off
enum class SaveErrorCode { InvalidData, AuthenticationFailed, EncryptionRequired, KeyUnavailable, Io, PlatformUnavailable, Outdated };
// clang-format on

class SaveError final : public std::runtime_error {
public:
    SaveError(SaveErrorCode code, const std::string& message);
    [[nodiscard]] SaveErrorCode Code() const noexcept;

private:
    SaveErrorCode _code;
};

struct SaveKeySettings {
    std::filesystem::path file;
};

struct SaveSettings {
    SaveEncryptionMode encryption = SaveEncryptionMode::Plaintext;
    std::size_t maxFileBytes = 1024 * 1024;
    bool keepBackup = false;
    SaveKeySettings keys;
    std::size_t maxDepth = 128;
    // Optional caller schema; false rejects a snapshot before load/save/recovery commits.
    std::function<bool(const JsonIO::Document::Object&)> acceptsValues;
};

// One owned generic value store per file. Construction and destruction never load/save.
class Persistence final {
public:
    using Document = JsonIO::Document;

    explicit Persistence(std::filesystem::path file, SaveSettings settings = {});
    ~Persistence();
    // When backups are enabled, Load automatically attempts verified recovery.
    SaveLoadResult Load();
    void Save() const;
    [[nodiscard]] std::optional<Document> Get(std::string_view key) const;
    void Set(std::string key, Document value);
    [[nodiscard]] bool Contains(std::string_view key) const;
    void Erase(std::string_view key);
    [[nodiscard]] std::int64_t GetInt(std::string_view key, std::int64_t fallback = 0) const;
    void SetInt(std::string key, std::int64_t value);
    [[nodiscard]] const std::filesystem::path& Path() const noexcept;
    [[nodiscard]] std::filesystem::path BackupPath() const;
    // Explicit application recovery decisions; neither operation changes loaded values.
    void ArchiveFiles();
    void RemoveFiles();
    // Invalidates ALL encrypted files sharing this key; never called automatically.
    void ResetEncryptionKey();

private:
    class State;
    std::unique_ptr<State> _state;

public:
    Persistence(const Persistence&) = delete;
    Persistence& operator=(const Persistence&) = delete;
    Persistence(Persistence&&) noexcept;
    Persistence& operator=(Persistence&&) noexcept;
};
