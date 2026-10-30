#include "Modules/Persistence/Persistence.hpp"
#include <utility>
#include "Modules/Persistence/Internals/State.h"

SaveError::SaveError(SaveErrorCode code, const std::string& message)
    : std::runtime_error(message)
    , _code(code) {}
SaveErrorCode SaveError::Code() const noexcept { return _code; }
Persistence::Persistence(std::filesystem::path file, SaveSettings settings)
    : _state(std::make_unique<State>(std::move(file), std::move(settings))) {}
Persistence::~Persistence() = default;
Persistence::Persistence(Persistence&&) noexcept = default;
Persistence& Persistence::operator=(Persistence&&) noexcept = default;
SaveLoadResult Persistence::Load() { return _state->Load(); }
void Persistence::Save() const { _state->Save(); }
std::optional<Persistence::Document> Persistence::Get(std::string_view key) const { return _state->Get(key); }
void Persistence::Set(std::string key, Document value) { _state->Set(std::move(key), std::move(value)); }
bool Persistence::Contains(std::string_view key) const { return _state->Contains(key); }
void Persistence::Erase(std::string_view key) { _state->Erase(key); }
std::int64_t Persistence::GetInt(std::string_view key, std::int64_t fallback) const {
    return _state->GetInt(key, fallback);
}
void Persistence::SetInt(std::string key, std::int64_t value) { Set(std::move(key), value); }
const std::filesystem::path& Persistence::Path() const noexcept { return _state->Path(); }
std::filesystem::path Persistence::BackupPath() const { return _state->BackupPath(); }
void Persistence::ArchiveFiles() { _state->ArchiveFiles(); }
void Persistence::RemoveFiles() { _state->RemoveFiles(); }
void Persistence::ResetEncryptionKey() { _state->ResetEncryptionKey(); }
