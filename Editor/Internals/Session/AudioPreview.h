#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// The Audio Preview panel's headless parts: folder neighbours, labels and the waveform.

// Sounds in the file's folder, by name; the file is among them when it exists.
[[nodiscard]] std::vector<std::filesystem::path> SoundsBeside(const std::filesystem::path& file);
// The sound `step` places away in its folder; none past either end.
[[nodiscard]] std::optional<std::filesystem::path> NeighborSound(const std::filesystem::path& file, int step);
// "1:05" or "0:07.4": tenths below ten seconds.
[[nodiscard]] std::string FormatAudioTime(float seconds);
[[nodiscard]] std::string FormatFileSize(std::uintmax_t bytes);

// One column per bucket, both in [0, 1]: the loudest sample and the root mean square of all channels.
struct AudioWaveform {
    std::vector<float> peaks;
    std::vector<float> rms;
    int sampleRate = 0;
    int channels = 0;
    float seconds = 0.0f;
};
// Fewer buckets than requested when the sound has fewer frames.
[[nodiscard]] AudioWaveform SummarizeWaveform(std::span<const float> samples, int channels, int sampleRate,
                                              std::size_t buckets);
