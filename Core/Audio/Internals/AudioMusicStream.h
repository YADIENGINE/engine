#pragma once
#include <raylib.h>
#include <filesystem>
#include "Audio/AudioSystem.hpp"

class AudioMusicStream {
public:
    AudioMusicStream(const std::filesystem::path& file, MusicVolumeGroup group);
    ~AudioMusicStream();
    AudioMusicStream(AudioMusicStream&& other) noexcept;
    AudioMusicStream& operator=(AudioMusicStream&& other) noexcept;
    [[nodiscard]] ::Music& Stream() { return _stream; }
    [[nodiscard]] const ::Music& Stream() const { return _stream; }
    [[nodiscard]] MusicVolumeGroup Group() const { return _group; }

private:
    void Release();

    ::Music _stream{};
    MusicVolumeGroup _group;

public:
    AudioMusicStream(const AudioMusicStream&) = delete;
    AudioMusicStream& operator=(const AudioMusicStream&) = delete;
};
