#pragma once
#include <filesystem>
#include "Audio/AudioSystem.hpp"

void ValidateAudioPath(const std::filesystem::path& file);
void ValidateAudioVolume(float volume);
void ValidateAudioPitch(float pitch);
void ValidateAudioEffectSettings(const EffectSettings& settings);
void ValidateAudioMusicGroup(MusicVolumeGroup group);
