#include "Audio/Internals/AudioGraph.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
constexpr ma_uint32 OfflineRate = 48000;
constexpr ma_uint32 CaptureRingSeconds = 5;
constexpr ma_uint32 Channels = 2;

void Require(ma_result result, const char* what) {
    if (result != MA_SUCCESS) throw std::runtime_error(std::string(what) + ": " + ma_result_description(result));
}
} // namespace

AudioGraph::AudioGraph(AudioOutput output)
    : _output(output) {
    ma_resource_manager_config resources = ma_resource_manager_config_init();
    resources.decodedFormat = ma_format_f32; // Rate and channels stay the file's own: see the class comment
    Require(ma_resource_manager_init(&resources, &_resources), "Could not start audio decoding");
    try {
        ma_engine_config config = ma_engine_config_init();
        config.pResourceManager = &_resources;
        config.channels = Channels;
        config.onProcess = &AudioGraph::OnProcess;
        config.pProcessUserData = this;
        if (output == AudioOutput::Offline) {
            config.noDevice = MA_TRUE;
            config.sampleRate = OfflineRate;
        }
        Require(ma_engine_init(&config, &_engine), "Could not initialize the audio device");
    } catch (...) {
        ma_resource_manager_uninit(&_resources);
        throw;
    }
    try {
        _master = std::make_unique<ShapeNode>(_engine, ma_engine_get_endpoint(&_engine), ShapeNode::Running::Always);
        for (auto& bus : _buses)
            bus = std::make_unique<ShapeNode>(_engine, _master->Node(), ShapeNode::Running::Always);
        _music = std::make_unique<ShapeNode>(_engine, Bus(SoundBus::Music).Node(), ShapeNode::Running::Always);
        _menuMusic = std::make_unique<ShapeNode>(_engine, Bus(SoundBus::Music).Node(), ShapeNode::Running::Always);
    } catch (...) {
        _menuMusic.reset();
        _music.reset();
        for (auto& bus : _buses)
            bus.reset();
        _master.reset();
        ma_engine_uninit(&_engine);
        ma_resource_manager_uninit(&_resources);
        throw;
    }
}

AudioGraph::~AudioGraph() {
    EndCapture();
    ma_engine_stop(&_engine); // Before the nodes, so that nothing runs through them while they go
    _menuMusic.reset();
    _music.reset();
    for (auto& bus : _buses)
        bus.reset();
    _master.reset();
    ma_engine_uninit(&_engine);
    ma_resource_manager_uninit(&_resources);
    if (_ringMade) ma_pcm_rb_uninit(&_ring);
}

int AudioGraph::SampleRate() const { return static_cast<int>(ma_engine_get_sample_rate(&_engine)); }
std::uint64_t AudioGraph::ClockFrames() const { return ma_engine_get_time_in_pcm_frames(&_engine); }

void AudioGraph::Render(std::span<float> stereo) {
    if (_output != AudioOutput::Offline) throw std::logic_error("Only an offline AudioSystem can Render");
    if (stereo.size() % Channels != 0) throw std::invalid_argument("Render needs whole stereo frames");
    ma_uint64 read = 0;
    Require(ma_engine_read_pcm_frames(&_engine, stereo.data(), stereo.size() / Channels, &read), "Could not render audio");
    std::fill(stereo.begin() + static_cast<std::ptrdiff_t>(read * Channels), stereo.end(), 0.0f);
}

void AudioGraph::OnProcess(void* self, float* frames, ma_uint64 count) {
    auto& graph = *static_cast<AudioGraph*>(self);
    if (!graph._capturing.load(std::memory_order_acquire)) return;
    while (count > 0) {
        ma_uint32 room = static_cast<ma_uint32>(std::min<ma_uint64>(count, UINT32_MAX));
        void* target = nullptr;
        if (ma_pcm_rb_acquire_write(&graph._ring, &room, &target) != MA_SUCCESS || room == 0) return; // Nobody draining
        std::copy_n(frames, room * Channels, static_cast<float*>(target));
        ma_pcm_rb_commit_write(&graph._ring, room);
        frames += room * Channels;
        count -= room;
    }
}

void AudioGraph::StartCapture(const std::filesystem::path& wavFile, float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0f) throw std::invalid_argument("A capture needs a positive length");
    EndCapture();
    if (!_ringMade) {
        Require(ma_pcm_rb_init(ma_format_f32, Channels, ma_engine_get_sample_rate(&_engine) * CaptureRingSeconds, nullptr,
                               nullptr, &_ring),
                "Could not prepare the capture");
        _ringMade = true;
    }
    const ma_encoder_config config =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, Channels, ma_engine_get_sample_rate(&_engine));
    _encoder.emplace();
#ifdef _WIN32
    const ma_result made = ma_encoder_init_file_w(wavFile.c_str(), &config, &*_encoder);
#else
    const ma_result made = ma_encoder_init_file(wavFile.c_str(), &config, &*_encoder);
#endif
    if (made != MA_SUCCESS) {
        _encoder.reset();
        throw std::runtime_error("Could not write the capture to " + wavFile.string() + ": " +
                                 ma_result_description(made));
    }
    _captureLeft = static_cast<std::uint64_t>(std::llround(static_cast<double>(seconds) * SampleRate()));
    _capturing.store(true, std::memory_order_release);
}

void AudioGraph::DrainCapture() {
    if (!_encoder) return;
    while (_captureLeft > 0) {
        ma_uint32 frames = static_cast<ma_uint32>(std::min<std::uint64_t>(_captureLeft, UINT32_MAX));
        void* source = nullptr;
        if (ma_pcm_rb_acquire_read(&_ring, &frames, &source) != MA_SUCCESS || frames == 0) return;
        ma_uint64 written = 0;
        ma_encoder_write_pcm_frames(&*_encoder, source, frames, &written);
        ma_pcm_rb_commit_read(&_ring, frames);
        _captureLeft -= frames;
    }
    EndCapture();
}

void AudioGraph::EndCapture() {
    _capturing.store(false, std::memory_order_release);
    if (_encoder) {
        ma_encoder_uninit(&*_encoder);
        _encoder.reset();
    }
    if (!_ringMade) return;
    // What is left in the ring belongs to the capture that ended. A block still being written is cleared by the next.
    for (;;) {
        ma_uint32 frames = UINT32_MAX;
        void* source = nullptr;
        if (ma_pcm_rb_acquire_read(&_ring, &frames, &source) != MA_SUCCESS || frames == 0) return;
        ma_pcm_rb_commit_read(&_ring, frames);
    }
}
