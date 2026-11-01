#include "Editor/Internals/Session/AudioWaveformCache.h"
#include <chrono>
#include <format>
#include <system_error>
#include <utility>

namespace {
// Changes when the file is saved again.
std::string FileVersion(const std::filesystem::path& file) {
    std::error_code error;
    const auto size = std::filesystem::file_size(file, error);
    if (error) return {};
    const auto written = std::filesystem::last_write_time(file, error);
    if (error) return {};
    return std::format("{}:{}", size, written.time_since_epoch().count());
}
bool Finished(const std::future<AudioWaveform>& decoding) {
    return decoding.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}
} // namespace

AudioWaveformCache::AudioWaveformCache(Decoder decoder)
    : _decoder(std::move(decoder)) {}

WaveformEntry AudioWaveformCache::Request(const std::filesystem::path& file) {
    std::erase_if(_replaced, Finished);
    const auto version = FileVersion(file);
    auto& slot = _slots[file];
    if (slot.version != version || (!slot.decoding.valid() && !slot.entry.waveform && slot.entry.error.empty())) {
        // A decode of the old version keeps running elsewhere: replacing its future would wait for it here.
        if (slot.decoding.valid()) _replaced.push_back(std::move(slot.decoding));
        slot.version = version;
        slot.entry = {};
        slot.decoding = std::async(std::launch::async, [decoder = _decoder, file] {
            const auto decoded = decoder(file);
            return SummarizeWaveform(decoded.samples, decoded.channels, decoded.sampleRate, Buckets);
        });
    }
    if (slot.decoding.valid() && Finished(slot.decoding)) {
        try {
            slot.entry = {WaveformState::Ready, std::make_shared<const AudioWaveform>(slot.decoding.get()), {}};
        } catch (const std::exception& error) {
            slot.entry = {WaveformState::Failed, nullptr, error.what()};
        }
    }
    return slot.entry;
}
