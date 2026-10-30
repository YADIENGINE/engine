#include "Modules/Persistence/Internals/State.h"
#include <utility>
#include "Platform/Internals/FileStorage.h"
#include "Platform/Internals/StorageError.h"

namespace {
constexpr std::int64_t SaveVersion = 2;

template <class Operation> decltype(auto) Checked(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const JsonIO::Error& error) {
        switch (error.Code()) {
        case JsonIO::ErrorCode::AuthenticationFailed:
            throw SaveError(SaveErrorCode::AuthenticationFailed, error.what());
        case JsonIO::ErrorCode::Io:
        case JsonIO::ErrorCode::MissingFile        : throw SaveError(SaveErrorCode::Io, error.what());
        case JsonIO::ErrorCode::PlatformUnavailable: throw SaveError(SaveErrorCode::PlatformUnavailable, error.what());
        default                                    : throw SaveError(SaveErrorCode::InvalidData, error.what());
        }
    } catch (const StorageError& error) {
        const auto code = error.Code() == StorageErrorCode::TooLarge              ? SaveErrorCode::InvalidData
                          : error.Code() == StorageErrorCode::PlatformUnavailable ? SaveErrorCode::PlatformUnavailable
                                                                                  : SaveErrorCode::Io;
        throw SaveError(code, error.what());
    } catch (const std::filesystem::filesystem_error& error) {
        throw SaveError(SaveErrorCode::Io, error.what());
    }
}
void ValidateKey(std::string_view key) {
    if (key.empty()) throw std::invalid_argument("Persistence keys must not be empty");
}
bool CanRecover(SaveErrorCode code) {
    return code != SaveErrorCode::Io && code != SaveErrorCode::PlatformUnavailable && code != SaveErrorCode::Outdated;
}
} // namespace

Persistence::State::State(std::filesystem::path file, SaveSettings settings)
    : _file(std::move(file))
    , _settings(std::move(settings)) {
    if (_file.empty() || _settings.maxFileBytes == 0 || _settings.maxDepth < 2)
        throw std::invalid_argument("Persistence requires a path, positive size limit and depth of at least two");
    _file = Checked([&] { return std::filesystem::absolute(_file); });
    if (_settings.encryption == SaveEncryptionMode::Encrypted) {
        if (_settings.keys.file.empty()) throw std::invalid_argument("Encrypted persistence requires a key file");
        _keys.emplace(_settings.keys.file);
    }
}
void Persistence::State::ValidateValues(const Document::Object& values) const {
    if (_settings.acceptsValues && !_settings.acceptsValues(values))
        throw SaveError(SaveErrorCode::InvalidData, "Persistence snapshot does not match the caller's schema");
}
Persistence::Document::Object Persistence::State::ReadValues(const std::filesystem::path& file) const {
    return Checked([&] {
        const JsonIO::Limits limits{_settings.maxFileBytes, _settings.maxDepth};
        Document record;
        bool encrypted = false;
        try {
            record = JsonIO::Read(file, {.limits = limits});
        } catch (const JsonIO::Error& error) {
            if (error.Code() != JsonIO::ErrorCode::EncryptionRequired) throw;
            if (!_keys)
                throw SaveError(SaveErrorCode::InvalidData, "Encrypted file cannot be loaded in plaintext mode");
            const auto key = _keys->Load(false);
            record = JsonIO::Read(file, {.limits = limits, .key = key.Bytes()});
            encrypted = true;
        }
        if (_keys && !encrypted)
            throw SaveError(SaveErrorCode::EncryptionRequired, "Plaintext save is not accepted in encrypted mode");
        const auto& object = record.Members();
        if (object.size() != 2 || !object.contains("version") || !object.contains("values"))
            throw SaveError(SaveErrorCode::InvalidData, "Invalid persistence record schema");
        const auto version = record.At("version").AsInt64();
        if (version >= 1 && version < SaveVersion)
            throw SaveError(SaveErrorCode::Outdated, "Outdated save version " + std::to_string(version));
        if (version != SaveVersion)
            throw SaveError(SaveErrorCode::InvalidData, "Unsupported persistence record version");
        auto values = std::move(record.At("values").Members());
        for (const auto& [key, value] : values) {
            (void)value;
            if (key.empty()) throw SaveError(SaveErrorCode::InvalidData, "Empty persistence key");
        }
        ValidateValues(values);
        return values;
    });
}
void Persistence::State::WriteRecord(const std::filesystem::path& file, const Document::Object& values) const {
    Checked([&] {
        const Document record = Document::Object{{"version", SaveVersion}, {"values", values}};
        const JsonIO::Limits limits{_settings.maxFileBytes, _settings.maxDepth};
        if (_keys) {
            const auto key = _keys->Load(true);
            JsonIO::Write(file, record, {.limits = limits, .key = key.Bytes()});
        } else
            JsonIO::Write(file, record, {.limits = limits});
    });
}
void Persistence::State::WriteValues(const Document::Object& values) const {
    Checked([&] {
        // Validate all caller-supplied values before touching the previous record or key store.
        ValidateValues(values);
        (void)JsonIO::Serialize(Document::Object{{"version", SaveVersion}, {"values", values}},
                                {.limits = {_settings.maxFileBytes, _settings.maxDepth}});
        if (_settings.keepBackup && std::filesystem::exists(_file)) {
            std::optional<Document::Object> previous;
            try {
                previous = ReadValues(_file);
            } catch (const SaveError& error) {
                if (error.Code() == SaveErrorCode::Io || error.Code() == SaveErrorCode::PlatformUnavailable ||
                    error.Code() == SaveErrorCode::KeyUnavailable)
                    throw;
            }
            if (previous) WriteRecord(BackupPath(), *previous);
        }
        WriteRecord(_file, values);
        // Separate atomic commit: failure here may follow a successful primary write.
        if (_settings.keepBackup) WriteRecord(BackupPath(), values);
    });
}
bool Persistence::State::TryRecover() {
    return Checked([&] {
        const auto backup = BackupPath();
        if (!std::filesystem::exists(backup)) return false;
        auto loaded = ReadValues(backup);
        if (std::filesystem::exists(_file)) (void)ArchiveInvalidFile(_file);
        WriteRecord(_file, loaded);
        _values.swap(loaded);
        return true;
    });
}
SaveLoadResult Persistence::State::Load() {
    return Checked([&] {
        if (!std::filesystem::exists(_file)) {
            if (_settings.keepBackup && TryRecover()) return SaveLoadResult::Recovered;
            _values.clear();
            return SaveLoadResult::Missing;
        }
        try {
            auto loaded = ReadValues(_file);
            _values.swap(loaded);
            return SaveLoadResult::Loaded;
        } catch (const SaveError& error) {
            if (!_settings.keepBackup || !CanRecover(error.Code())) throw;
            try {
                if (TryRecover()) return SaveLoadResult::Recovered;
            } catch (const SaveError& recoveryError) {
                if (!CanRecover(recoveryError.Code())) throw;
            }
            throw;
        }
    });
}
void Persistence::State::Save() const { WriteValues(_values); }
std::optional<Persistence::Document> Persistence::State::Get(std::string_view key) const {
    ValidateKey(key);
    const auto found = _values.find(key);
    if (found == _values.end()) return std::nullopt;
    return found->second;
}
void Persistence::State::Set(std::string key, Document value) {
    ValidateKey(key);
    _values.insert_or_assign(std::move(key), std::move(value));
}
bool Persistence::State::Contains(std::string_view key) const {
    ValidateKey(key);
    return _values.contains(key);
}
void Persistence::State::Erase(std::string_view key) {
    ValidateKey(key);
    const auto found = _values.find(key);
    if (found != _values.end()) _values.erase(found);
}
std::int64_t Persistence::State::GetInt(std::string_view key, std::int64_t fallback) const {
    return Checked([&] {
        ValidateKey(key);
        const auto found = _values.find(key);
        return found == _values.end() ? fallback : found->second.AsInt64();
    });
}
const std::filesystem::path& Persistence::State::Path() const noexcept { return _file; }
std::filesystem::path Persistence::State::BackupPath() const {
    auto result = _file;
    result += ".bak";
    return result;
}
void Persistence::State::ArchiveFiles() {
    Checked([&] {
        if (std::filesystem::exists(_file)) (void)ArchiveInvalidFile(_file);
        if (std::filesystem::exists(BackupPath())) (void)ArchiveInvalidFile(BackupPath());
    });
}
void Persistence::State::RemoveFiles() {
    Checked([&] {
        std::filesystem::remove(_file);
        std::filesystem::remove(BackupPath());
    });
}
void Persistence::State::ResetEncryptionKey() {
    if (!_keys) throw std::logic_error("Plaintext persistence has no encryption key");
    Checked([&] { _keys->Reset(); });
}
