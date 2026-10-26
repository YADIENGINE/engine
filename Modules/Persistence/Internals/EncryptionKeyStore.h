#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include "Modules/Persistence/Internals/EncryptionKey.h"

struct SaveKeySettings;

// Android supplies Keystore wrapping; desktop defaults use DPAPI or an owner-only file.
class EncryptionKeyStore final {
public:
    using Wrapper = std::function<std::string(std::string_view, bool)>;
    explicit EncryptionKeyStore(std::filesystem::path file, Wrapper wrapper = {}, std::function<void()> reset = {});
    [[nodiscard]] EncryptionKey Load(bool create) const;
    // Explicit recovery only: archive the unusable key, then reset its wrapping key.
    void Reset() const;

private:
    std::filesystem::path _file;
    Wrapper _wrapper;
    std::function<void()> _reset;
};

[[nodiscard]] EncryptionKeyStore CreatePersistenceKeyStore(const SaveKeySettings& settings);
