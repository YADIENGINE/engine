#pragma once
#include <filesystem>
#include "Audio/AudioSystem.h"

void ValidateAudioPath(const std::filesystem::path& file);
// Tools' source files (LoadMusicFile): absolute paths only, so they never resolve against the working directory.
void ValidateAudioFilePath(const std::filesystem::path& file);
void ValidateAudioSeek(float seconds);
// Where a valid seek lands: past the end goes to the end.
[[nodiscard]] float AudioSeekPosition(float seconds, float length);
void ValidateAudioVolume(float volume);
void ValidateAudioPitch(float pitch);
void ValidateAudioEffectSettings(const EffectSettings& settings);
void ValidateAudioMusicGroup(MusicVolumeGroup group);
