#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>
#include "Audio/AudioSystem.h"

// For tools that work on source files, such as an editor previewing assets. Games load through AudioSystem, whose
// paths stay relative to the asset root.

// Interleaved samples in [-1, 1].
struct DecodedAudio {
    int sampleRate = 0;
    int channels = 0;
    std::vector<float> samples;
    [[nodiscard]] std::size_t Frames() const {
        return channels > 0 ? samples.size() / static_cast<std::size_t>(channels) : 0;
    }
};

// Reads a whole file without an audio device; safe on any thread. Throws std::runtime_error if it cannot be read.
[[nodiscard]] DecodedAudio DecodeAudioFile(const std::filesystem::path& file);
// AudioSystem::LoadMusic for a file anywhere on disk. The path must be absolute (std::invalid_argument otherwise).
void LoadMusicFile(AudioSystem& audio, MusicId id, const std::filesystem::path& file);
