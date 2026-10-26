#include "Audio/Internals/AudioVolumePolicy.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include "Options/GameOptions.h"

namespace {
constexpr std::int64_t FullPercent = 100;
} // namespace

AudioVolumePolicy::AudioVolumePolicy(AudioVolumeSettings settings)
    : _settings(std::move(settings)) {
    if (_settings.steps <= 0 || FullPercent % _settings.steps != 0)
        throw std::invalid_argument("Audio volume steps must be a positive divisor of 100");
}
int AudioVolumePolicy::NearestStep(float volume) const {
    return std::clamp(static_cast<int>(std::lround(volume * _settings.steps)), 0, _settings.steps);
}
float AudioVolumePolicy::Step(float volume, int steps) const {
    const auto step = std::clamp(static_cast<std::int64_t>(NearestStep(volume)) + steps, std::int64_t{0},
                                 static_cast<std::int64_t>(_settings.steps));
    return static_cast<float>(step) / _settings.steps;
}
int AudioVolumePolicy::Percent(float volume) { return static_cast<int>(std::lround(volume * 100.0f)); }
float AudioVolumePolicy::Load(std::int64_t percent) const {
    if (percent < 0 || percent > FullPercent || percent % (FullPercent / _settings.steps) != 0) return 1.0f;
    return static_cast<float>(percent) / 100.0f;
}
float AudioVolumePolicy::LoadEffects(const GameOptions& options) const {
    return Load(options.Get(EngineOption::EffectsVolume, FullPercent));
}
float AudioVolumePolicy::LoadMusic(const GameOptions& options) const {
    return Load(options.Get(EngineOption::MusicVolume, FullPercent));
}
float AudioVolumePolicy::LoadMenuMusic(const GameOptions& options) const {
    return Load(options.Get(EngineOption::MenuMusicVolume, FullPercent));
}
void AudioVolumePolicy::Store(GameOptions& options, float effects, float music, float menuMusic) const {
    const std::int64_t stepPercent = FullPercent / _settings.steps;
    const auto store = [&](EngineOption option, float volume) {
        const std::int64_t percent = NearestStep(volume) * stepPercent;
        // Hiç yazılmamış varsayılan (%100) yazılmaz: sese dokunmayan oyun dosya oluşturmaz.
        if (options.Contains(option) || percent != FullPercent) options.Set(option, percent);
    };
    store(EngineOption::EffectsVolume, effects);
    store(EngineOption::MusicVolume, music);
    store(EngineOption::MenuMusicVolume, menuMusic);
}
