#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include "Modules/Persistence/Persistence.hpp"

// Engine'in kendi kaydettiği tercihler; dosya anahtarları GameOptions.cpp tablosunda.
// clang-format off
enum class EngineOption { EffectsVolume, MusicVolume, MenuMusicVolume, FpsLimit, BorderlessFullscreen, WindowWidth, WindowHeight, RenderHeight, VSync, ShowFps, Glow, ParticlePercent, Shake, SprintEffects, Flashes, BackgroundEffects, Count };
// clang-format on
// Missing: dosya yoktu ya da eski şema silindi. Recovered: yedekten kurtarıldı.
// Reset: bozuk dosya arşivlendi, boş başlandı.
enum class OptionsLoadResult { Missing, Loaded, Recovered, Reset };

// Oyuncunun elle düzenleyebileceği tek, şifresiz tercih dosyası; değerler tamsayıdır. Engine tercihleri EngineOption
// ile, oyununkiler kendi anahtarlarıyla yazılır; engine anahtarlarının ad alanları (ilk noktaya kadar) oyuna kapalıdır.
// Yazma sadece Save'de ve sadece değişiklik varsa olur.
class GameOptions {
public:
    explicit GameOptions(std::filesystem::path file);
    // Okunamayan dosyayı arşivleyip boş başlar; sadece IO / platform hataları fırlatır.
    OptionsLoadResult Load();
    [[nodiscard]] OptionsLoadResult LoadResult() const { return _loadResult; }

    [[nodiscard]] std::int64_t GetInt(std::string_view key, std::int64_t fallback = 0) const;
    // Engine ad alanındaki anahtar std::invalid_argument fırlatır.
    void SetInt(std::string key, std::int64_t value);
    [[nodiscard]] bool Contains(std::string_view key) const;
    void Erase(std::string_view key);

    [[nodiscard]] std::int64_t Get(EngineOption option, std::int64_t fallback) const;
    [[nodiscard]] bool Contains(EngineOption option) const;
    void Set(EngineOption option, std::int64_t value);

    // Değişiklik varsa dosyayı yazar.
    bool Save();
    [[nodiscard]] bool Dirty() const { return _dirty; }

private:
    void Write(std::string key, std::int64_t value);

    Persistence _save;
    OptionsLoadResult _loadResult = OptionsLoadResult::Missing;
    bool _dirty = false;
};
