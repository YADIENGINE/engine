#include "Audio/Internals/Validation.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>

void ValidateAudioPath(const std::filesystem::path& file) {
    if (file.empty() || file.has_root_path())
        throw std::invalid_argument("Audio file path must be relative to the asset root: " + file.string());
    for (const auto& part : file) {
        if (part == "..") throw std::invalid_argument("Audio file path cannot leave the asset root: " + file.string());
    }
}
void ValidateAudioVolume(float volume) {
    if (!(volume >= 0.0f && volume <= 1.0f))
        throw std::invalid_argument("Audio volume must be finite and between 0 and 1");
}
void ValidateAudioPitch(float pitch) {
    if (!std::isfinite(pitch) || pitch <= 0.0f) throw std::invalid_argument("Audio pitch must be finite and positive");
}
void ValidateAudioEffectSettings(const EffectSettings& settings) {
    if (settings.maxConcurrent == 0) throw std::invalid_argument("Effect maxConcurrent must be at least 1");
    ValidateAudioVolume(settings.volume);
    switch (settings.overflow) {
    case EffectOverflow::RestartNext:
    case EffectOverflow::Skip       : return;
    }
    throw std::invalid_argument("Invalid effect overflow policy");
}
void ValidateAudioMusicGroup(MusicVolumeGroup group) {
    switch (group) {
    case MusicVolumeGroup::Default:
    case MusicVolumeGroup::Menu   : return;
    }
    throw std::invalid_argument("Invalid music volume group");
}
void AudioSystem::ValidateCatalog(const AudioCatalog& catalog) {
    std::unordered_set<SoundId> effects;
    for (const auto& effect : catalog.effects) {
        if (!effects.insert(effect.id).second)
            throw std::invalid_argument("Duplicate sound ID: " + std::to_string(static_cast<std::size_t>(effect.id)));
        ValidateAudioPath(effect.file);
        ValidateAudioEffectSettings(effect.settings);
    }
    std::unordered_set<MusicId> music;
    for (const auto& track : catalog.music) {
        if (!music.insert(track.id).second)
            throw std::invalid_argument("Duplicate music ID: " + std::to_string(static_cast<std::size_t>(track.id)));
        ValidateAudioPath(track.file);
        ValidateAudioMusicGroup(track.volumeGroup);
    }
}
