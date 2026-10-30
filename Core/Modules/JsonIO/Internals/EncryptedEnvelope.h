#pragma once
#include "Modules/JsonIO/JsonIO.hpp"

class EncryptedEnvelope final {
public:
    [[nodiscard]] static bool Matches(std::string_view bytes);
    static void Validate(std::string_view bytes);
    [[nodiscard]] static std::string Protect(std::string_view plaintext, std::span<const std::uint8_t> key);
    [[nodiscard]] static std::string Unprotect(std::string_view bytes, std::span<const std::uint8_t> key);
    static void ValidateKey(std::span<const std::uint8_t> key);
};
