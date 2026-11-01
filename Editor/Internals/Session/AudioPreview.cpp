#include "Editor/Internals/Session/AudioPreview.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <system_error>
#include "Editor/Internals/Session/AssetLibrary.h"

namespace {
constexpr float TenthsBelowSeconds = 10.0f;
constexpr double KiB = 1024.0;
} // namespace

std::vector<std::filesystem::path> SoundsBeside(const std::filesystem::path& file) {
    std::vector<std::filesystem::path> sounds;
    std::error_code error;
    for (std::filesystem::directory_iterator it(file.parent_path(), error), end; !error && it != end;
         it.increment(error))
        if (it->is_regular_file(error) && IsAudioFile(it->path())) sounds.push_back(it->path());
    std::ranges::sort(sounds, [](const auto& a, const auto& b) { return a.filename() < b.filename(); });
    return sounds;
}

std::optional<std::filesystem::path> NeighborSound(const std::filesystem::path& file, int step) {
    const auto sounds = SoundsBeside(file);
    const auto found = std::ranges::find(sounds, file.filename(), &std::filesystem::path::filename);
    if (found == sounds.end()) return std::nullopt;
    const auto index = (found - sounds.begin()) + step;
    if (index < 0 || index >= std::ssize(sounds)) return std::nullopt;
    return sounds[static_cast<std::size_t>(index)];
}

std::string FormatAudioTime(float seconds) {
    seconds = std::max(seconds, 0.0f);
    if (seconds < TenthsBelowSeconds) {
        const int tenths = static_cast<int>(seconds * 10.0f);
        return std::format("0:0{}.{}", tenths / 10, tenths % 10);
    }
    const int whole = static_cast<int>(seconds);
    return std::format("{}:{:02}", whole / 60, whole % 60);
}

std::string FormatFileSize(std::uintmax_t bytes) {
    if (bytes < 1024) return std::format("{} B", bytes);
    const double kib = static_cast<double>(bytes) / KiB;
    if (kib < KiB) return std::format("{:.1f} KB", kib);
    return std::format("{:.1f} MB", kib / KiB);
}

AudioWaveform SummarizeWaveform(std::span<const float> samples, int channels, int sampleRate, std::size_t buckets) {
    AudioWaveform result{{}, {}, sampleRate, channels, 0.0f};
    if (channels <= 0 || buckets == 0) return result;
    const auto width = static_cast<std::size_t>(channels);
    const std::size_t frames = samples.size() / width;
    if (sampleRate > 0) result.seconds = static_cast<float>(frames) / static_cast<float>(sampleRate);
    const std::size_t count = std::min(buckets, frames);
    result.peaks.resize(count);
    result.rms.resize(count);
    for (std::size_t bucket = 0; bucket < count; ++bucket) {
        const std::size_t first = bucket * frames / count;
        const std::size_t last = (bucket + 1) * frames / count;
        float peak = 0.0f;
        double sum = 0.0;
        for (std::size_t i = first * width; i < last * width; ++i) {
            const float value = std::min(std::abs(samples[i]), 1.0f);
            peak = std::max(peak, value);
            sum += static_cast<double>(value) * value;
        }
        result.peaks[bucket] = peak;
        result.rms[bucket] = static_cast<float>(std::sqrt(sum / static_cast<double>((last - first) * width)));
    }
    return result;
}
