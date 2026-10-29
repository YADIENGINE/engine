#pragma once
#include <stdexcept>
#include <string>

enum class StorageErrorCode { Io, TooLarge, PlatformUnavailable };

class StorageError final : public std::runtime_error {
public:
    StorageError(StorageErrorCode code, const std::string& message);
    [[nodiscard]] StorageErrorCode Code() const noexcept;

private:
    StorageErrorCode _code;
};
