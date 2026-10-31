#pragma once
#include <raylib.h>
#include <cstddef>
#include <filesystem>
#include <vector>
#include "Audio/AudioSystem.h"

// Owns the sample and its aliases. Aliases must be released before the source.
class EffectVoices {
public:
    EffectVoices(const std::filesystem::path& file, EffectSettings settings);
    ~EffectVoices();
    EffectVoices(EffectVoices&& other) noexcept;
    EffectVoices& operator=(EffectVoices&& other) noexcept;
    void Play(float pitch, bool onlyIfIdle = false);
    void Stop();
    void SetVolume(float volume);
    void ApplyVolume(float effectsVolume) const;

private:
    void Release();

    std::vector<Sound> _voices;
    std::size_t _nextVoice = 0;
    float _volume;
    EffectOverflow _overflow;

public:
    EffectVoices(const EffectVoices&) = delete;
    EffectVoices& operator=(const EffectVoices&) = delete;
};
