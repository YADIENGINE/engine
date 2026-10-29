#include "Platform/Internals/StorageError.h"

StorageError::StorageError(StorageErrorCode code, const std::string& message)
    : std::runtime_error(message)
    , _code(code) {}
StorageErrorCode StorageError::Code() const noexcept { return _code; }
