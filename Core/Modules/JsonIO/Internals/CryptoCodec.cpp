#include "Modules/JsonIO/Internals/CryptoCodec.h"
#include <monocypher.h>

void CryptoCodec::Seal(std::span<std::uint8_t> output, std::span<std::uint8_t, 16> tag,
                       std::span<const std::uint8_t, 32> key, std::span<const std::uint8_t, 24> nonce,
                       std::string_view header, std::string_view plaintext) {
    crypto_aead_lock(output.data(), tag.data(), key.data(), nonce.data(),
                     reinterpret_cast<const std::uint8_t*>(header.data()), header.size(),
                     reinterpret_cast<const std::uint8_t*>(plaintext.data()), plaintext.size());
}
bool CryptoCodec::Open(std::span<std::uint8_t> output, std::span<const std::uint8_t, 16> tag,
                       std::span<const std::uint8_t, 32> key, std::span<const std::uint8_t, 24> nonce,
                       std::string_view header, std::string_view ciphertext) {
    return crypto_aead_unlock(output.data(), tag.data(), key.data(), nonce.data(),
                              reinterpret_cast<const std::uint8_t*>(header.data()), header.size(),
                              reinterpret_cast<const std::uint8_t*>(ciphertext.data()), ciphertext.size()) == 0;
}
