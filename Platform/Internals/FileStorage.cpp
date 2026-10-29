#include "Platform/Internals/FileStorage.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <memory>
#include <system_error>
#include "Platform/Internals/StorageError.h"
#include "Platform/Internals/SecureRandom.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
std::filesystem::path UniquePath(const std::filesystem::path& file, const char* suffix) {
    std::array<std::uint8_t, 16> random{};
    FillSecureRandom(random);
    auto result = file;
    result += suffix;
    constexpr std::string_view Hex = "0123456789abcdef";
    for (const auto byte : random) {
        result += Hex[byte >> 4];
        result += Hex[byte & 15];
    }
    return result;
}
[[noreturn]] void IoFailure(const std::filesystem::path& file) {
    throw StorageError(StorageErrorCode::Io, "File operation failed: " + file.string());
}
} // namespace

std::string ReadBinaryFile(const std::filesystem::path& file, std::size_t maxBytes) {
    std::ifstream input(file, std::ios::binary);
    if (!input) IoFailure(file);
    std::string result;
    std::array<char, 4096> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = static_cast<std::size_t>(input.gcount());
        if (count > maxBytes - result.size())
            throw StorageError(StorageErrorCode::TooLarge, "File exceeds size limit: " + file.string());
        result.append(buffer.data(), count);
    }
    if (input.bad() || !input.eof()) IoFailure(file);
    return result;
}

bool WriteBinaryFileAtomic(const std::filesystem::path& file, std::string_view bytes, FileAccess access,
                           WriteMode mode) {
    const bool privateFile = access == FileAccess::OwnerOnly;
    const bool replace = mode == WriteMode::Replace;
    const auto temporary = UniquePath(file, ".tmp.");
    try {
        const auto parent = file.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent);
#ifdef _WIN32
        (void)privateFile; // Key contents are protected by DPAPI.
        const HANDLE handle =
            CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) IoFailure(temporary);
        const auto close = [](void* value) { CloseHandle(value); };
        std::unique_ptr<void, decltype(close)> output(handle, close);
        while (!bytes.empty()) {
            const auto size = static_cast<DWORD>(std::min(bytes.size(), std::size_t{1024 * 1024}));
            DWORD written = 0;
            if (!WriteFile(handle, bytes.data(), size, &written, nullptr) || written == 0) IoFailure(temporary);
            bytes.remove_prefix(written);
        }
        if (!FlushFileBuffers(handle)) IoFailure(temporary);
        output.reset();
        const DWORD flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), file.c_str(), flags)) {
            const auto error = GetLastError();
            if (!replace && (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)) {
                std::filesystem::remove(temporary);
                return false;
            }
            IoFailure(file);
        }
#else
        if (privateFile && !parent.empty() && chmod(parent.c_str(), S_IRWXU) != 0) IoFailure(parent);
        const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
                                    privateFile ? S_IRUSR | S_IWUSR : S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (descriptor < 0) IoFailure(temporary);
        FILE* stream = fdopen(descriptor, "wb");
        if (!stream) {
            ::close(descriptor);
            IoFailure(temporary);
        }
        std::unique_ptr<FILE, decltype(&fclose)> output(stream, &fclose);
        while (!bytes.empty()) {
            const auto written = ::write(descriptor, bytes.data(), std::min(bytes.size(), std::size_t{1024 * 1024}));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) IoFailure(temporary);
            bytes.remove_prefix(static_cast<std::size_t>(written));
        }
        if (fsync(descriptor) != 0) IoFailure(temporary);
        output.reset();
        if (replace) {
            if (::rename(temporary.c_str(), file.c_str()) != 0) IoFailure(file);
        } else {
            if (::link(temporary.c_str(), file.c_str()) != 0) {
                if (errno != EEXIST) IoFailure(file);
                std::filesystem::remove(temporary);
                return false;
            }
            std::filesystem::remove(temporary);
        }
#endif
        return true;
    } catch (const std::filesystem::filesystem_error& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw StorageError(StorageErrorCode::Io, error.what());
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::filesystem::path ArchiveInvalidFile(const std::filesystem::path& file) {
    const auto destination = UniquePath(file, ".invalid.");
    try {
#ifdef _WIN32
        if (!MoveFileExW(file.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH)) IoFailure(file);
#else
        if (::link(file.c_str(), destination.c_str()) != 0) IoFailure(file);
        if (::unlink(file.c_str()) != 0) IoFailure(file);
#endif
        return destination;
    } catch (const std::filesystem::filesystem_error& error) {
        throw StorageError(StorageErrorCode::Io, error.what());
    }
}
