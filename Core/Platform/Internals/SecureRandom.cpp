#include "Platform/Internals/SecureRandom.h"
#include <algorithm>
#include <cerrno>
#include <limits>
#include "Platform/Internals/StorageError.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#elif defined(__linux__)
#include <sys/random.h>
#endif

void FillSecureRandom(std::span<std::uint8_t> bytes) {
#ifdef _WIN32
    while (!bytes.empty()) {
        const auto count =
            static_cast<ULONG>(std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<ULONG>::max())));
        if (BCryptGenRandom(nullptr, bytes.data(), count, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
            throw StorageError(StorageErrorCode::PlatformUnavailable, "System random generator failed");
        bytes = bytes.subspan(count);
    }
#elif defined(__linux__)
    while (!bytes.empty()) {
        const auto count = getrandom(bytes.data(), std::min(bytes.size(), std::size_t{256}), 0);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw StorageError(StorageErrorCode::PlatformUnavailable, "System random generator failed");
        bytes = bytes.subspan(static_cast<std::size_t>(count));
    }
#else
    (void)bytes;
    throw StorageError(StorageErrorCode::PlatformUnavailable,
                       "Secure random generator is unavailable on this platform");
#endif
}
