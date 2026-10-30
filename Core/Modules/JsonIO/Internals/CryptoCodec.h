#pragma once
#include <cstdint>
#include <span>
#include <string_view>

// XChaCha20-Poly1305 primitive adapter. The enclosing module owns the file format.
class CryptoCodec final {
public:
    static void Seal(std::span<std::uint8_t> output, std::span<std::uint8_t, 16> tag,
                     std::span<const std::uint8_t, 32> key, std::span<const std::uint8_t, 24> nonce,
                     std::string_view header, std::string_view plaintext);
    [[nodiscard]] static bool Open(std::span<std::uint8_t> output, std::span<const std::uint8_t, 16> tag,
                                   std::span<const std::uint8_t, 32> key, std::span<const std::uint8_t, 24> nonce,
                                   std::string_view header, std::string_view ciphertext);
};
