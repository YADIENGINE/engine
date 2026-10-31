#include "Modules/Persistence/Internals/EncryptionKeyStore.h"
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include "Modules/Persistence/Persistence.h"
#include "Modules/Persistence/Internals/SensitiveMemory.h"
#include "Platform/Internals/FileStorage.h"
#include "Platform/Internals/StorageError.h"
#include "Platform/Internals/SecureRandom.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dpapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t KeyBytes = 32;
#ifdef _WIN32
constexpr std::size_t MaxWrappedKeyBytes = 4096;
#endif
std::string DesktopWrap(std::string_view bytes, bool encrypt) {
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))};
    DATA_BLOB output{};
    const BOOL success =
        encrypt ? CryptProtectData(&input, L"Engine save key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                   &output)
                : CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!success) {
        const auto error = GetLastError();
        throw SaveError(!encrypt && (error == ERROR_INVALID_DATA || error == ERROR_INVALID_PARAMETER)
                            ? SaveErrorCode::KeyUnavailable
                            : SaveErrorCode::PlatformUnavailable,
                        "DPAPI key protection failed: " + std::to_string(error));
    }
    const auto release = [size = output.cbData](BYTE* value) {
        SecureZeroMemory(value, size);
        LocalFree(value);
    };
    const std::unique_ptr<BYTE, decltype(release)> blob(output.pbData, release);
    return std::string(reinterpret_cast<const char*>(blob.get()), output.cbData);
#else
    (void)encrypt;
    return std::string(bytes);
#endif
}
std::string ReadKey(const std::filesystem::path& file) {
#ifndef _WIN32
    const int descriptor = open(file.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (descriptor < 0) throw SaveError(SaveErrorCode::Io, "Could not open encryption key");
    FILE* stream = fdopen(descriptor, "rb");
    if (!stream) {
        close(descriptor);
        throw SaveError(SaveErrorCode::Io, "Could not read encryption key");
    }
    const std::unique_ptr<FILE, decltype(&fclose)> input(stream, &fclose);
    struct stat info{};
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != geteuid())
        throw SaveError(SaveErrorCode::Io, "Encryption key must be an owned regular file");
    if (fchmod(descriptor, S_IRUSR | S_IWUSR) != 0)
        throw SaveError(SaveErrorCode::Io, "Could not restrict encryption key permissions");
    if (info.st_size != static_cast<off_t>(KeyBytes))
        throw SaveError(SaveErrorCode::KeyUnavailable, "Invalid encryption key length");
    std::string result(KeyBytes, '\0');
    if (fread(result.data(), 1, result.size(), stream) != result.size()) {
        WipeKeyMemory(result.data(), result.size());
        throw SaveError(SaveErrorCode::Io, "Could not read encryption key");
    }
    return result;
#else
    return ReadBinaryFile(file, MaxWrappedKeyBytes);
#endif
}
} // namespace

EncryptionKeyStore::EncryptionKeyStore(std::filesystem::path file)
    : _file(std::move(file)) {
    if (_file.empty()) throw std::invalid_argument("Encryption key path must not be empty");
    _file = std::filesystem::absolute(_file);
}
EncryptionKey EncryptionKeyStore::Load(bool create) const {
    try {
        if (!std::filesystem::exists(_file)) {
            if (!create) throw SaveError(SaveErrorCode::KeyUnavailable, "Encryption key is missing");
            std::array<std::uint8_t, KeyBytes> random{};
            FillSecureRandom(random);
            std::string raw(reinterpret_cast<const char*>(random.data()), random.size());
            WipeKeyMemory(random.data(), random.size());
            std::string wrapped;
            try {
                wrapped = DesktopWrap(raw, true);
            } catch (...) {
                WipeKeyMemory(raw.data(), raw.size());
                throw;
            }
            WipeKeyMemory(raw.data(), raw.size());
            try {
                WriteBinaryFileAtomic(_file, wrapped, FileAccess::OwnerOnly, WriteMode::CreateNew);
            } catch (...) {
                WipeKeyMemory(wrapped.data(), wrapped.size());
                throw;
            }
            WipeKeyMemory(wrapped.data(), wrapped.size());
        }
        std::string wrapped;
        try {
            wrapped = ReadKey(_file);
        } catch (const StorageError& error) {
            if (error.Code() == StorageErrorCode::TooLarge)
                throw SaveError(SaveErrorCode::KeyUnavailable, "Invalid wrapped encryption key");
            throw;
        }
        std::string raw;
        try {
            raw = DesktopWrap(wrapped, false);
        } catch (...) {
            WipeKeyMemory(wrapped.data(), wrapped.size());
            throw;
        }
        WipeKeyMemory(wrapped.data(), wrapped.size());
        return EncryptionKey(std::move(raw));
    } catch (const std::filesystem::filesystem_error& error) {
        throw SaveError(SaveErrorCode::Io, error.what());
    }
}
void EncryptionKeyStore::Reset() const {
    try {
        if (std::filesystem::exists(_file)) (void)ArchiveInvalidFile(_file);
    } catch (const std::filesystem::filesystem_error& error) {
        throw SaveError(SaveErrorCode::Io, error.what());
    }
}
