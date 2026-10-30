#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// Stateless JSON/file facade. Document is an owned public value, never a backend object.
class JsonIO final {
public:
    // clang-format off
    enum class ErrorCode { InvalidJson, TypeMismatch, MissingMember, NumberOutOfRange, LimitExceeded, MissingFile, Io, PlatformUnavailable, EncryptionRequired, InvalidEnvelope, AuthenticationFailed, InvalidOptions };
    // clang-format on
    enum class Format { Pretty, Compact };
    // Gzip: write-only RFC 1952 member; Read does not decompress and it cannot be combined with a key.
    enum class Compression { None, Gzip };

    class Error final : public std::runtime_error {
    public:
        Error(ErrorCode code, const std::string& message, std::size_t byte = 0);
        [[nodiscard]] ErrorCode Code() const noexcept;
        // One-based byte position when supplied by the parser; zero means unavailable.
        [[nodiscard]] std::size_t Byte() const noexcept;

    private:
        ErrorCode _code;
        std::size_t _byte;
    };

    struct Document {
        using Object = std::map<std::string, Document, std::less<>>;
        using Array = std::vector<Document>;
        using Value =
            std::variant<std::nullptr_t, bool, std::int64_t, std::uint64_t, double, std::string, Object, Array>;

        Document() = default;
        // Value conversions intentionally support object/array initializer lists.
        Document(std::nullptr_t);
        Document(bool item);
        template <std::signed_integral T>
            requires(!std::same_as<T, bool>)
        Document(T number)
            : value(static_cast<std::int64_t>(number)) {}
        template <std::unsigned_integral T>
            requires(!std::same_as<T, bool>)
        Document(T number)
            : value(static_cast<std::uint64_t>(number)) {}
        Document(double number);
        Document(const char* text);
        Document(std::string text);
        Document(std::string_view text);
        Document(Object object);
        Document(Array array);

        [[nodiscard]] static Document MakeObject();
        [[nodiscard]] static Document MakeArray();
        Document& operator[](std::string key);
        [[nodiscard]] const Document& At(std::string_view key) const;
        [[nodiscard]] Document& At(std::string_view key);
        void Append(Document item);
        [[nodiscard]] const Object& Members() const;
        [[nodiscard]] Object& Members();
        [[nodiscard]] const Array& Elements() const;
        [[nodiscard]] Array& Elements();
        [[nodiscard]] std::int64_t AsInt64() const;
        [[nodiscard]] std::uint64_t AsUInt64() const;
        [[nodiscard]] double AsDouble() const;
        [[nodiscard]] const std::string& AsString() const;
        [[nodiscard]] bool AsBool() const;
        [[nodiscard]] bool IsNull() const;
        [[nodiscard]] bool operator==(const Document& other) const;

        Value value = nullptr;
    };

    struct Limits {
        std::size_t maxBytes = 1024 * 1024;
        std::size_t maxDepth = 128;
    };
    struct ReadOptions {
        Limits limits;
        // Empty means plaintext; otherwise exactly 32 borrowed bytes, valid for this call only.
        std::span<const std::uint8_t> key;
    };
    struct SerializeOptions {
        Limits limits;
        Format format = Format::Pretty;
    };
    struct WriteOptions {
        Limits limits;
        std::span<const std::uint8_t> key;
        Format format = Format::Pretty;
        Compression compression = Compression::None;
    };

    [[nodiscard]] static Document Read(const std::filesystem::path& file);
    [[nodiscard]] static Document Read(const std::filesystem::path& file, const ReadOptions& options);
    static void Write(const std::filesystem::path& file, const Document& document);
    static void Write(const std::filesystem::path& file, const Document& document, const WriteOptions& options);
    [[nodiscard]] static Document Parse(std::string_view text);
    [[nodiscard]] static Document Parse(std::string_view text, Limits limits);
    [[nodiscard]] static std::string Serialize(const Document& document);
    [[nodiscard]] static std::string Serialize(const Document& document, const SerializeOptions& options);

public:
    JsonIO() = delete;
};
