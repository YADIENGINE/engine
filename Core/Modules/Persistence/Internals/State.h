#pragma once
#include "Modules/Persistence/Persistence.h"
#include "Modules/Persistence/Internals/EncryptionKeyStore.h"

class Persistence::State final {
public:
    State(std::filesystem::path file, SaveSettings settings);
    SaveLoadResult Load();
    void Save() const;
    [[nodiscard]] std::optional<Document> Get(std::string_view key) const;
    void Set(std::string key, Document value);
    [[nodiscard]] bool Contains(std::string_view key) const;
    void Erase(std::string_view key);
    [[nodiscard]] std::int64_t GetInt(std::string_view key, std::int64_t fallback) const;
    [[nodiscard]] const std::filesystem::path& Path() const noexcept;
    [[nodiscard]] std::filesystem::path BackupPath() const;
    void ArchiveFiles();
    void RemoveFiles();
    void ResetEncryptionKey();

private:
    void ValidateValues(const Document::Object& values) const;
    [[nodiscard]] Document::Object ReadValues(const std::filesystem::path& file) const;
    void WriteRecord(const std::filesystem::path& file, const Document::Object& values) const;
    void WriteValues(const Document::Object& values) const;
    bool TryRecover();

    std::filesystem::path _file;
    SaveSettings _settings;
    std::optional<EncryptionKeyStore> _keys;
    Document::Object _values;
};
