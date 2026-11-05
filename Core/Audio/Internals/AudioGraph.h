#pragma once
#include <miniaudio.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include "Audio/AudioSystem.h"
#include "Audio/Internals/ShapeNode.h"

// miniaudio's engine with the stops every sound passes through: voice -> bus -> master -> device. The file's own rate
// is kept when a sound is decoded or streamed, and each voice resamples as it plays: converted at load, a file that is
// not at the device's rate starts with a few zeros, and those come round again at every loop (measured; D39).
class AudioGraph {
public:
    explicit AudioGraph(AudioOutput output);
    ~AudioGraph();

    [[nodiscard]] ma_engine& Engine() { return _engine; }
    [[nodiscard]] int SampleRate() const;
    [[nodiscard]] std::uint64_t ClockFrames() const;
    [[nodiscard]] ShapeNode& Master() { return *_master; }
    // A bus's level and muffling, under the volume options. World and Ui also carry the effects option; Music has one
    // stop before it per option.
    [[nodiscard]] ShapeNode& Bus(SoundBus bus) { return *_buses[static_cast<std::size_t>(bus)]; }
    [[nodiscard]] ShapeNode& MusicOption(MusicVolumeGroup group) {
        return group == MusicVolumeGroup::Menu ? *_menuMusic : *_music;
    }
    // Where a sound of this bus attaches.
    [[nodiscard]] ma_node* Input(SoundBus bus) {
        return bus == SoundBus::Music ? _music->Node() : Bus(bus).Node();
    }

    void Render(std::span<float> stereo);

    void StartCapture(const std::filesystem::path& wavFile, float seconds);
    [[nodiscard]] bool IsCapturing() const { return _encoder.has_value(); }
    void DrainCapture();

private:
    static void OnProcess(void* self, float* frames, ma_uint64 count);
    void EndCapture();

    AudioOutput _output;
    ma_resource_manager _resources{};
    ma_engine _engine{};
    std::unique_ptr<ShapeNode> _master;
    std::array<std::unique_ptr<ShapeNode>, AllSoundBuses.size()> _buses;
    std::unique_ptr<ShapeNode> _music;
    std::unique_ptr<ShapeNode> _menuMusic;

    // Capture: the audio thread writes into the ring, the main thread drains it into the file.
    bool _ringMade = false;
    ma_pcm_rb _ring{};
    std::atomic<bool> _capturing{false};
    std::optional<ma_encoder> _encoder;
    std::uint64_t _captureLeft = 0;

public:
    AudioGraph(const AudioGraph&) = delete;
    AudioGraph& operator=(const AudioGraph&) = delete;
};
