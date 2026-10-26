#include "Audio/Internals/EffectVoices.h"
#include <algorithm>
#include <stdexcept>

EffectVoices::EffectVoices(const std::filesystem::path& file, EffectSettings settings)
    : _volume(settings.volume)
    , _overflow(settings.overflow) {
    _voices.reserve(settings.maxConcurrent);
    const Sound source = LoadSound(file.string().c_str());
    if (!IsSoundValid(source)) throw std::runtime_error("Could not load sound: " + file.string());
    _voices.push_back(source);
    try {
        for (std::size_t i = 1; i < settings.maxConcurrent; ++i) {
            const Sound alias = LoadSoundAlias(source);
            if (!IsSoundValid(alias)) throw std::runtime_error("Could not create sound voice: " + file.string());
            _voices.push_back(alias);
        }
    } catch (...) {
        Release();
        throw;
    }
}
EffectVoices::~EffectVoices() { Release(); }
EffectVoices::EffectVoices(EffectVoices&& other) noexcept
    : _nextVoice(other._nextVoice)
    , _volume(other._volume)
    , _overflow(other._overflow) {
    _voices.swap(other._voices);
}
EffectVoices& EffectVoices::operator=(EffectVoices&& other) noexcept {
    if (this == &other) return *this;
    Release();
    _voices.swap(other._voices);
    _nextVoice = other._nextVoice;
    _volume = other._volume;
    _overflow = other._overflow;
    return *this;
}
void EffectVoices::Release() {
    Stop();
    for (std::size_t i = _voices.size(); i > 1; --i)
        UnloadSoundAlias(_voices[i - 1]);
    if (!_voices.empty()) UnloadSound(_voices.front());
    _voices.clear();
    _nextVoice = 0;
}
void EffectVoices::Play(float pitch, bool onlyIfIdle) {
    if (onlyIfIdle && std::ranges::any_of(_voices, [](Sound voice) { return IsSoundPlaying(voice); })) return;
    for (std::size_t offset = 0; offset < _voices.size(); ++offset) {
        const std::size_t index = (_nextVoice + offset) % _voices.size();
        if (!IsSoundPlaying(_voices[index])) {
            SetSoundPitch(_voices[index], pitch);
            PlaySound(_voices[index]);
            _nextVoice = (index + 1) % _voices.size();
            return;
        }
    }
    if (_overflow == EffectOverflow::Skip) return;
    SetSoundPitch(_voices[_nextVoice], pitch);
    PlaySound(_voices[_nextVoice]);
    _nextVoice = (_nextVoice + 1) % _voices.size();
}
void EffectVoices::Stop() {
    for (const Sound voice : _voices)
        StopSound(voice);
}
void EffectVoices::SetVolume(float volume) { _volume = volume; }
void EffectVoices::ApplyVolume(float effectsVolume) const {
    for (const Sound voice : _voices)
        SetSoundVolume(voice, effectsVolume * _volume);
}
