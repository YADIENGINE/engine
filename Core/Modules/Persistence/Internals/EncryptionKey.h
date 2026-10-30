#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

class EncryptionKey final {
public:
    explicit EncryptionKey(std::string bytes);
    ~EncryptionKey();
    [[nodiscard]] std::span<const std::uint8_t, 32> Bytes() const { return _bytes; }

private:
    std::array<std::uint8_t, 32> _bytes{};

public:
    EncryptionKey(const EncryptionKey&) = delete;
    EncryptionKey& operator=(const EncryptionKey&) = delete;
    EncryptionKey(EncryptionKey&&) = delete;
    EncryptionKey& operator=(EncryptionKey&&) = delete;
};
