#include <catch2/catch_test_macros.hpp>
#include <array>
#include <functional>
#include <limits>
#include "Tests/Support/GzipInflate.h"
#include "Tests/Support/TempDirectory.h"
#include "Modules/JsonIO/JsonIO.hpp"
#include "Platform/Internals/FileStorage.h"

namespace {
using D = JsonIO::Document;
void ExpectError(const std::function<void()>& action, JsonIO::ErrorCode code) {
    try {
        action();
        FAIL("Expected JsonIO::Error");
    } catch (const JsonIO::Error& error) {
        CHECK(error.Code() == code);
    }
}
} // namespace

TEST_CASE("JsonIO.Document.OwnedValuesStrictAccessAndIntegerRanges", "[json-io]") {
    D document = D::Object{{"items", D::Array{}}, {"nested", D::Object{{"enabled", true}}}};
    document["items"].Append(D::Object{{"text", "Türkçe \"\\\n"}});
    document["nested"]["enabled"] = false;
    document["minimum"] = std::numeric_limits<std::int64_t>::min();
    document["maximum"] = std::numeric_limits<std::uint64_t>::max();
    document["positive_unsigned"] = std::uint64_t{42};
    document["fraction"] = 1.25;
    const auto parsed = JsonIO::Parse(JsonIO::Serialize(document));
    CHECK(parsed == document);
    CHECK(parsed.At("minimum").AsInt64() == std::numeric_limits<std::int64_t>::min());
    CHECK(parsed.At("maximum").AsUInt64() == std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE(parsed.At("nested").At("enabled").AsBool());
    ExpectError([&] { (void)parsed.At("absent"); }, JsonIO::ErrorCode::MissingMember);
    ExpectError([&] { (void)parsed.At("maximum").AsInt64(); }, JsonIO::ErrorCode::NumberOutOfRange);
    ExpectError([&] { (void)parsed.At("fraction").AsInt64(); }, JsonIO::ErrorCode::TypeMismatch);
    D null;
    ExpectError([&] { null["x"] = 1; }, JsonIO::ErrorCode::TypeMismatch);
    CHECK(null.IsNull());
}

TEST_CASE("JsonIO.Codec.StrictJsonUnicodeAndResourceLimits", "[json-io]") {
    // MSVC rejects surrogate UCNs even inside raw strings, so the backslash is escaped.
    CHECK(JsonIO::Parse("\"\\uD83D\\uDE80\"").AsString() == "🚀");
    CHECK(JsonIO::Parse("null").IsNull());
    ExpectError([&] { (void)JsonIO::Parse(std::string_view("null\0trailing", 13)); }, JsonIO::ErrorCode::InvalidJson);
    CHECK(JsonIO::Parse("[true,1,1.5]").Elements().size() == 3);
    for (const auto text : {R"({"x":1,"x":2})", R"({"a":{"x":1,"x":2}})", R"({"a":1,"\u0061":2})", "{} trailing",
                            "[1,]", "/* comment */ {}", R"("\uD800")"}) {
        CAPTURE(text);
        CHECK_THROWS_AS(JsonIO::Parse(text), JsonIO::Error);
    }
    ExpectError([&] { (void)JsonIO::Parse("18446744073709551616"); }, JsonIO::ErrorCode::NumberOutOfRange);
    std::string invalid = "\"";
    invalid.push_back(static_cast<char>(0xff));
    invalid += '"';
    CHECK_THROWS_AS(JsonIO::Parse(invalid), JsonIO::Error);
    CHECK_THROWS_AS(JsonIO::Serialize(D(invalid)), JsonIO::Error);
    ExpectError([&] { (void)JsonIO::Parse(" 1 ", {.maxBytes = 2}); }, JsonIO::ErrorCode::LimitExceeded);
    ExpectError([&] { (void)JsonIO::Parse("[[]]", {.maxDepth = 1}); }, JsonIO::ErrorCode::LimitExceeded);
    // D::Array{D::Array{}} would move the inner vector instead of nesting it.
    ExpectError([&] { (void)JsonIO::Serialize(D::Array{D(D::Array{})}, {.limits = {.maxDepth = 1}}); },
                JsonIO::ErrorCode::LimitExceeded);
    ExpectError([&] { (void)JsonIO::Serialize("abc", {.limits = {.maxBytes = 4}}); }, JsonIO::ErrorCode::LimitExceeded);
    CHECK_THROWS_AS(JsonIO::Serialize(std::numeric_limits<double>::infinity()), JsonIO::Error);
}

TEST_CASE("JsonIO.Files.PlaintextEncryptedTamperingAndAtomicFailure", "[json-io][encryption]") {
    TempDirectory temp;
    const auto file = temp.Path() / "document.json";
    std::array<std::uint8_t, 32> key{};
    key.fill(0x42);
    const D document = D::Object{{"message", "private"}, {"items", D::Array{1, true, nullptr}}};
    ExpectError([&] { (void)JsonIO::Read(file); }, JsonIO::ErrorCode::MissingFile);
    JsonIO::Write(file, document);
    CHECK(JsonIO::Read(file) == document);
    ExpectError([&] { (void)JsonIO::Read(file, {.key = key}); }, JsonIO::ErrorCode::InvalidEnvelope);
    JsonIO::Write(file, document, {.key = key});
    const auto trusted = ReadBinaryFile(file);
    CHECK(trusted.starts_with("ENGSAVE!"));
    CHECK(trusted.find("private") == std::string::npos);
    CHECK(JsonIO::Read(file, {.key = key}) == document);
    ExpectError([&] { (void)JsonIO::Read(file); }, JsonIO::ErrorCode::EncryptionRequired);
    JsonIO::Write(file, document, {.key = key});
    CHECK(ReadBinaryFile(file) != trusted);
    auto wrongKey = key;
    wrongKey[0] ^= 1;
    ExpectError([&] { (void)JsonIO::Read(file, {.key = wrongKey}); }, JsonIO::ErrorCode::AuthenticationFailed);
    for (const auto offset : {std::size_t{8}, std::size_t{12}, std::size_t{36}, trusted.size() - 1}) {
        auto damaged = trusted;
        damaged[offset] ^= 1;
        WriteBinaryFileAtomic(file, damaged);
        CHECK_THROWS_AS(JsonIO::Read(file, {.key = key}), JsonIO::Error);
    }
    WriteBinaryFileAtomic(file, trusted.substr(0, 40));
    ExpectError([&] { (void)JsonIO::Read(file); }, JsonIO::ErrorCode::InvalidEnvelope);
    CHECK_THROWS_AS(JsonIO::Read(file, {.key = key}), JsonIO::Error);
    WriteBinaryFileAtomic(file, trusted);
    CHECK_THROWS_AS(JsonIO::Write(file, std::numeric_limits<double>::infinity(), {.key = key}), JsonIO::Error);
    CHECK(ReadBinaryFile(file) == trusted);
    ExpectError([&] { JsonIO::Write(file, document, {.limits = {.maxBytes = trusted.size() - 1}, .key = key}); },
                JsonIO::ErrorCode::LimitExceeded);
    CHECK(ReadBinaryFile(file) == trusted);
    const auto occupied = temp.Path() / "occupied";
    std::filesystem::create_directory(occupied);
    ExpectError([&] { JsonIO::Write(occupied, document, {.key = key}); }, JsonIO::ErrorCode::Io);
    for (const auto& item : std::filesystem::directory_iterator(temp.Path()))
        CHECK(item.path().filename().string().find(".tmp.") == std::string::npos);
}
TEST_CASE("JsonIO.Files.GzipWritesStandardMemberAndRejectsUnsupportedUse", "[json-io]") {
    TempDirectory temp;
    const auto file = temp.Path() / "document.json.gz";
    const JsonIO::WriteOptions gzip{.format = JsonIO::Format::Compact, .compression = JsonIO::Compression::Gzip};
    CHECK(ReferenceCrc32("123456789") == 0xcbf43926u);
    SECTION("Small document keeps CRC32 and byte count") {
        const D document = D::Object{{"label", "Türkçe \"\\n"}, {"score", 123}};
        JsonIO::Write(file, document, gzip);
        const auto bytes = ReadBinaryFile(file);
        const auto text = InflateGzip(bytes);
        CHECK(text == JsonIO::Serialize(document, {.format = JsonIO::Format::Compact}));
        CHECK(LittleEndian(std::string_view(bytes).substr(bytes.size() - 8, 4)) == ReferenceCrc32(text));
        CHECK(LittleEndian(std::string_view(bytes).substr(bytes.size() - 4)) == text.size());
    }
    SECTION("Repeated UTF-8 across deflate blocks") {
        D document = D::MakeArray();
        for (int i = 0; i < 20'000; ++i)
            document.Append(D::Object{{"world", 10}, {"score", 123}, {"label", "Türkçe"}});
        const JsonIO::WriteOptions large{.limits = {.maxBytes = std::numeric_limits<std::size_t>::max()},
                                         .format = JsonIO::Format::Compact,
                                         .compression = JsonIO::Compression::Gzip};
        JsonIO::Write(file, document, large);
        const auto bytes = ReadBinaryFile(file);
        const auto text = InflateGzip(bytes);
        CHECK(text == JsonIO::Serialize(document, {.limits = large.limits, .format = JsonIO::Format::Compact}));
        CHECK(bytes.size() < text.size() / 10);
    }
    SECTION("Read does not decompress and encryption cannot be combined") {
        JsonIO::Write(file, D::Object{{"score", 1}}, gzip);
        ExpectError([&] { (void)JsonIO::Read(file); }, JsonIO::ErrorCode::InvalidJson);
        std::array<std::uint8_t, 32> key{};
        key.fill(0x42);
        const auto other = temp.Path() / "encrypted.json.gz";
        ExpectError([&] { JsonIO::Write(other, D::Object{}, {.key = key, .compression = JsonIO::Compression::Gzip}); },
                    JsonIO::ErrorCode::InvalidOptions);
        CHECK_FALSE(std::filesystem::exists(other));
    }
}
