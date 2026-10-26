#include "Options/GameOptions.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
#include "Utilities/EnumTable.h"

namespace {
struct EngineOptionRow {
    EngineOption id;
    std::string_view key;
};
// Mevcut kayıtlar bu anahtarları kullanır; değiştirmek oyuncunun ayarını sıfırlar.
constexpr std::array EngineOptionRows{
    EngineOptionRow{EngineOption::EffectsVolume, "audio.effects_percent"},
    EngineOptionRow{EngineOption::MusicVolume, "audio.music_percent"},
    EngineOptionRow{EngineOption::MenuMusicVolume, "audio.menu_music_percent"},
    EngineOptionRow{EngineOption::FpsLimit, "display.fps_limit"},
    EngineOptionRow{EngineOption::BorderlessFullscreen, "display.borderless_fullscreen"},
    EngineOptionRow{EngineOption::WindowWidth, "display.window_width"},
    EngineOptionRow{EngineOption::WindowHeight, "display.window_height"},
    EngineOptionRow{EngineOption::RenderHeight, "display.render_height"},
    EngineOptionRow{EngineOption::VSync, "display.vsync"},
    EngineOptionRow{EngineOption::ShowFps, "display.show_fps"},
    EngineOptionRow{EngineOption::Glow, "graphics.glow"},
    EngineOptionRow{EngineOption::ParticlePercent, "graphics.particles_percent"},
    EngineOptionRow{EngineOption::Shake, "graphics.shake"},
    EngineOptionRow{EngineOption::SprintEffects, "graphics.sprint_effects"},
    EngineOptionRow{EngineOption::Flashes, "graphics.flashes"},
    EngineOptionRow{EngineOption::BackgroundEffects, "graphics.background_effects"},
};
static_assert(RowsInIdOrder(EngineOptionRows));

std::string_view KeyOf(EngineOption option) { return EngineOptionRows[static_cast<std::size_t>(option)].key; }
std::string_view NamespaceOf(std::string_view key) { return key.substr(0, key.find('.')); }
void CheckGameKey(std::string_view key) {
    const bool reserved = std::ranges::any_of(
        EngineOptionRows, [&](const EngineOptionRow& row) { return NamespaceOf(row.key) == NamespaceOf(key); });
    if (reserved) throw std::invalid_argument("Option key is in an engine namespace");
}
SaveSettings OptionsSaveSettings() {
    return {.encryption = SaveEncryptionMode::Plaintext,
            .keepBackup = true,
            .acceptsValues = [](const JsonIO::Document::Object& values) {
                try {
                    for (const auto& [key, value] : values) {
                        (void)key;
                        (void)value.AsInt64();
                    }
                    return true;
                } catch (const JsonIO::Error&) {
                    return false;
                }
            }};
}
} // namespace

GameOptions::GameOptions(std::filesystem::path file)
    : _save(std::move(file), OptionsSaveSettings()) {}

OptionsLoadResult GameOptions::Load() {
    _dirty = false;
    try {
        switch (_save.Load()) {
        case SaveLoadResult::Missing  : _loadResult = OptionsLoadResult::Missing; break;
        case SaveLoadResult::Loaded   : _loadResult = OptionsLoadResult::Loaded; break;
        case SaveLoadResult::Recovered: _loadResult = OptionsLoadResult::Recovered; break;
        }
    } catch (const SaveError& error) {
        switch (error.Code()) {
        case SaveErrorCode::Io:
        case SaveErrorCode::PlatformUnavailable: throw;
        case SaveErrorCode::Outdated:
            // Eski şema: dosya ve yedeği uyarısız silinir, ilk açılış gibi başlanır.
            _save.RemoveFiles();
            (void)_save.Load();
            _loadResult = OptionsLoadResult::Missing;
            break;
        case SaveErrorCode::InvalidData:
        case SaveErrorCode::AuthenticationFailed:
        case SaveErrorCode::EncryptionRequired:
        case SaveErrorCode::KeyUnavailable:
            _save.ArchiveFiles();
            _save.Save();
            _loadResult = OptionsLoadResult::Reset;
            break;
        }
    }
    return _loadResult;
}

std::int64_t GameOptions::GetInt(std::string_view key, std::int64_t fallback) const {
    return _save.GetInt(key, fallback);
}
void GameOptions::SetInt(std::string key, std::int64_t value) {
    CheckGameKey(key);
    Write(std::move(key), value);
}
bool GameOptions::Contains(std::string_view key) const { return _save.Contains(key); }
void GameOptions::Erase(std::string_view key) {
    CheckGameKey(key);
    if (!_save.Contains(key)) return;
    _save.Erase(key);
    _dirty = true;
}

std::int64_t GameOptions::Get(EngineOption option, std::int64_t fallback) const {
    return _save.GetInt(KeyOf(option), fallback);
}
bool GameOptions::Contains(EngineOption option) const { return _save.Contains(KeyOf(option)); }
void GameOptions::Set(EngineOption option, std::int64_t value) { Write(std::string(KeyOf(option)), value); }

void GameOptions::Write(std::string key, std::int64_t value) {
    if (_save.Contains(key) && _save.GetInt(key) == value) return;
    _save.SetInt(std::move(key), value);
    _dirty = true;
}

bool GameOptions::Save() {
    if (!_dirty) return false;
    _save.Save();
    _dirty = false;
    return true;
}
