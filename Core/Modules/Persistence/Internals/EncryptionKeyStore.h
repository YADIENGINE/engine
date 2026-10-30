#pragma once
#include <filesystem>
#include "Modules/Persistence/Internals/EncryptionKey.h"

// Keys use DPAPI on Windows or an owner-only file on other desktop platforms.
class EncryptionKeyStore final {
public:
    explicit EncryptionKeyStore(std::filesystem::path file);
    [[nodiscard]] EncryptionKey Load(bool create) const;
    // Explicit recovery only: archive the unusable key.
    void Reset() const;

private:
    std::filesystem::path _file;
};
