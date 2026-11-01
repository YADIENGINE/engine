#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "Audio/AudioFiles.h"
#include "Editor/Internals/Session/AudioPreview.h"

enum class WaveformState { Decoding, Ready, Failed };
struct WaveformEntry {
    WaveformState state = WaveformState::Decoding;
    std::shared_ptr<const AudioWaveform> waveform; // Set when Ready
    std::string error;                             // Set when Failed
};

// Decodes on background threads and keeps every summary in memory. A file decodes again once its size or write time
// changes. Use from one thread; the destructor waits for running decodes.
class AudioWaveformCache {
public:
    using Decoder = std::function<DecodedAudio(const std::filesystem::path&)>;
    static constexpr std::size_t Buckets = 1024;

    explicit AudioWaveformCache(Decoder decoder);
    // Starts decoding on the first request for a version of the file.
    [[nodiscard]] WaveformEntry Request(const std::filesystem::path& file);

private:
    struct Slot {
        std::string version;
        std::future<AudioWaveform> decoding; // Valid until it finishes
        WaveformEntry entry;
    };
    Decoder _decoder;
    std::map<std::filesystem::path, Slot> _slots;
    std::vector<std::future<AudioWaveform>> _replaced; // Decodes of old versions, dropped once they finish
};
