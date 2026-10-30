#include "Audio/Internals/AudioMusicStream.h"
#include <stdexcept>
#include <utility>

AudioMusicStream::AudioMusicStream(const std::filesystem::path& file, MusicVolumeGroup group)
    : _stream(LoadMusicStream(file.string().c_str()))
    , _group(group) {
    if (!IsMusicValid(_stream)) throw std::runtime_error("Could not load music: " + file.string());
}
AudioMusicStream::~AudioMusicStream() { Release(); }
AudioMusicStream::AudioMusicStream(AudioMusicStream&& other) noexcept
    : _stream(std::exchange(other._stream, {}))
    , _group(other._group) {}
AudioMusicStream& AudioMusicStream::operator=(AudioMusicStream&& other) noexcept {
    if (this == &other) return *this;
    Release();
    _stream = std::exchange(other._stream, {});
    _group = other._group;
    return *this;
}
void AudioMusicStream::Release() {
    if (!IsMusicValid(_stream)) return;
    StopMusicStream(_stream);
    UnloadMusicStream(_stream);
    _stream = {};
}
