#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include "Modules/Persistence/Persistence.h"

// Options saved by the engine itself; file keys are in the GameOptions.cpp table.
// clang-format off
enum class EngineOption { EffectsVolume, MusicVolume, MenuMusicVolume, FpsLimit, BorderlessFullscreen, WindowWidth, WindowHeight, RenderHeight, VSync, ShowFps, Glow, ParticlePercent, Shake, MotionEffects, Flashes, BackgroundEffects, Bloom, Count };
// clang-format on
// Missing: file was absent or an old schema was removed. Recovered: restored from backup.
// Reset: corrupt file was archived and an empty state was started.
enum class OptionsLoadResult { Missing, Loaded, Recovered, Reset };

// One unencrypted options file editable by the player; values are integers. Engine options use EngineOption;
// game options use their own keys. Engine key namespaces (up to the first dot) are reserved from the game.
// Writing occurs only in Save and only when there are changes.
class GameOptions {
public:
    explicit GameOptions(std::filesystem::path file);
    // Archives unreadable files and starts empty; throws only on IO / platform errors.
    OptionsLoadResult Load();
    [[nodiscard]] OptionsLoadResult LoadResult() const { return _loadResult; }

    [[nodiscard]] std::int64_t GetInt(std::string_view key, std::int64_t fallback = 0) const;
    // A key in an engine namespace throws std::invalid_argument.
    void SetInt(std::string key, std::int64_t value);
    [[nodiscard]] bool Contains(std::string_view key) const;
    void Erase(std::string_view key);

    [[nodiscard]] std::int64_t Get(EngineOption option, std::int64_t fallback) const;
    [[nodiscard]] bool Contains(EngineOption option) const;
    void Set(EngineOption option, std::int64_t value);

    // Writes the file if there are changes.
    bool Save();
    [[nodiscard]] bool Dirty() const { return _dirty; }

private:
    void Write(std::string key, std::int64_t value);

    Persistence _save;
    OptionsLoadResult _loadResult = OptionsLoadResult::Missing;
    bool _dirty = false;
};
