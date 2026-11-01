#include "Audio/AudioFiles.h"
#include "Audio/Internals/AudioDecoder.h"
#include "Audio/Internals/State.h"

DecodedAudio DecodeAudioFile(const std::filesystem::path& file) { return DecodeAudioSamples(file); }
void LoadMusicFile(AudioSystem& audio, MusicId id, const std::filesystem::path& file) {
    audio._state->LoadMusicFile(id, file, MusicVolumeGroup::Default);
}
