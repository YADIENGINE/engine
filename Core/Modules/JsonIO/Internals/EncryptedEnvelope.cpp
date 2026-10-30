#include "Modules/JsonIO/Internals/EncryptedEnvelope.h"
#include <algorithm>
#include <array>
#include "Modules/JsonIO/Internals/CryptoCodec.h"
#include "Platform/Internals/SecureRandom.h"

namespace {
constexpr std::string_view Magic = "ENGSAVE!";
constexpr std::size_t HeaderBytes = 12;
constexpr std::size_t NonceBytes = 24;
constexpr std::size_t TagBytes = 16;
constexpr std::size_t EnvelopeBytes = HeaderBytes + NonceBytes + TagBytes;
constexpr std::array<std::uint8_t, HeaderBytes> Header{'E', 'N', 'G', 'S', 'A', 'V', 'E', '!', 1, 0, 0, 0};
std::span<std::uint8_t> MutableBytes(std::string& bytes) {
    return {reinterpret_cast<std::uint8_t*>(bytes.data()), bytes.size()};
}
std::span<const std::uint8_t> Bytes(std::string_view bytes) {
    return {reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()};
}
} // namespace

bool EncryptedEnvelope::Matches(std::string_view bytes) { return bytes.starts_with(Magic); }
void EncryptedEnvelope::Validate(std::string_view bytes) {
    if (bytes.size() < EnvelopeBytes || !std::equal(Header.begin(), Header.end(), Bytes(bytes).begin()))
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidEnvelope, "Invalid or unsupported encrypted JSON header");
}
void EncryptedEnvelope::ValidateKey(std::span<const std::uint8_t> key) {
    if (!key.empty() && key.size() != 32)
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidOptions, "Encryption requires a 32-byte key");
}
std::string EncryptedEnvelope::Protect(std::string_view plaintext, std::span<const std::uint8_t> key) {
    ValidateKey(key);
    if (key.empty()) throw JsonIO::Error(JsonIO::ErrorCode::InvalidOptions, "Encryption key is missing");
    if (plaintext.size() > std::string{}.max_size() - EnvelopeBytes)
        throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "Encrypted JSON output is too large");
    std::string result(plaintext.size() + EnvelopeBytes, '\0');
    auto output = MutableBytes(result);
    std::copy(Header.begin(), Header.end(), output.begin());
    auto nonce = output.subspan<HeaderBytes, NonceBytes>();
    FillSecureRandom(nonce);
    CryptoCodec::Seal(output.subspan(EnvelopeBytes), output.subspan<HeaderBytes + NonceBytes, TagBytes>(),
                      key.first<32>(), nonce, std::string_view(result).substr(0, HeaderBytes), plaintext);
    return result;
}
std::string EncryptedEnvelope::Unprotect(std::string_view bytes, std::span<const std::uint8_t> key) {
    ValidateKey(key);
    if (key.empty()) throw JsonIO::Error(JsonIO::ErrorCode::EncryptionRequired, "Decryption key is missing");
    Validate(bytes);
    const auto input = Bytes(bytes);
    std::string result(bytes.size() - EnvelopeBytes, '\0');
    if (!CryptoCodec::Open(MutableBytes(result), input.subspan<HeaderBytes + NonceBytes, TagBytes>(), key.first<32>(),
                           input.subspan<HeaderBytes, NonceBytes>(), bytes.substr(0, HeaderBytes),
                           bytes.substr(EnvelopeBytes)))
        throw JsonIO::Error(JsonIO::ErrorCode::AuthenticationFailed, "Encrypted JSON authentication failed");
    return result;
}
