#include "Modules/JsonIO/JsonIO.hpp"
#include "Modules/JsonIO/Internals/EncryptedEnvelope.h"
#include "Modules/JsonIO/Internals/GzipCodec.h"
#include "Modules/JsonIO/Internals/JsonCodec.h"
#include "Platform/Internals/FileStorage.h"
#include "Platform/Internals/StorageError.h"

namespace {
[[noreturn]] void Translate(const StorageError& error) {
    switch (error.Code()) {
    case StorageErrorCode::Io      : throw JsonIO::Error(JsonIO::ErrorCode::Io, error.what());
    case StorageErrorCode::TooLarge: throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, error.what());
    case StorageErrorCode::PlatformUnavailable:
        throw JsonIO::Error(JsonIO::ErrorCode::PlatformUnavailable, error.what());
    }
    throw JsonIO::Error(JsonIO::ErrorCode::Io, error.what());
}
void ValidateFile(const std::filesystem::path& file, JsonIO::Limits limits) {
    if (file.empty() || limits.maxBytes == 0 || limits.maxDepth == 0)
        throw JsonIO::Error(JsonIO::ErrorCode::InvalidOptions, "JSON path and positive limits are required");
}
} // namespace

JsonIO::Document JsonIO::Read(const std::filesystem::path& file) { return Read(file, ReadOptions{}); }
JsonIO::Document JsonIO::Read(const std::filesystem::path& file, const ReadOptions& options) {
    ValidateFile(file, options.limits);
    EncryptedEnvelope::ValidateKey(options.key);
    try {
        if (!std::filesystem::exists(file))
            throw Error(ErrorCode::MissingFile, "JSON file does not exist: " + file.string());
        auto bytes = ReadBinaryFile(file, options.limits.maxBytes);
        if (options.key.empty()) {
            if (EncryptedEnvelope::Matches(bytes)) {
                EncryptedEnvelope::Validate(bytes);
                throw Error(ErrorCode::EncryptionRequired, "Encrypted JSON requires a key");
            }
        } else
            bytes = EncryptedEnvelope::Unprotect(bytes, options.key);
        return Parse(bytes, options.limits);
    } catch (const StorageError& error) {
        Translate(error);
    } catch (const std::filesystem::filesystem_error& error) {
        throw Error(ErrorCode::Io, error.what());
    }
}
void JsonIO::Write(const std::filesystem::path& file, const Document& document) {
    Write(file, document, WriteOptions{});
}
void JsonIO::Write(const std::filesystem::path& file, const Document& document, const WriteOptions& options) {
    ValidateFile(file, options.limits);
    EncryptedEnvelope::ValidateKey(options.key);
    const bool gzip = options.compression == Compression::Gzip;
    if (gzip && !options.key.empty())
        throw Error(ErrorCode::InvalidOptions, "Gzip compression cannot be combined with encryption");
    auto bytes = Serialize(document, {.limits = options.limits, .format = options.format});
    try {
        if (gzip) bytes = GzipCodec::Encode(bytes);
        if (!options.key.empty()) bytes = EncryptedEnvelope::Protect(bytes, options.key);
        if (bytes.size() > options.limits.maxBytes)
            throw Error(ErrorCode::LimitExceeded, "JSON file exceeds the stored byte limit");
        (void)WriteBinaryFileAtomic(file, bytes);
    } catch (const StorageError& error) {
        Translate(error);
    } catch (const std::filesystem::filesystem_error& error) {
        throw Error(ErrorCode::Io, error.what());
    }
}
JsonIO::Document JsonIO::Parse(std::string_view text) { return Parse(text, Limits{}); }
JsonIO::Document JsonIO::Parse(std::string_view text, Limits limits) { return JsonCodec::Parse(text, limits); }
std::string JsonIO::Serialize(const Document& document) { return Serialize(document, SerializeOptions{}); }
std::string JsonIO::Serialize(const Document& document, const SerializeOptions& options) {
    return JsonCodec::Serialize(document, options);
}
