#include "Audio/Internals/AudioDecoder.h"
#include <raylib.h>
#include <stdexcept>
#include <string>

DecodedAudio DecodeAudioSamples(const std::filesystem::path& file) {
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Audio file not found: " + file.string());
    ::Wave wave = LoadWave(file.string().c_str());
    if (!IsWaveValid(wave)) throw std::runtime_error("Could not decode audio: " + file.string());
    float* samples = LoadWaveSamples(wave);
    DecodedAudio decoded{static_cast<int>(wave.sampleRate), static_cast<int>(wave.channels), {}};
    if (samples) decoded.samples.assign(samples, samples + static_cast<std::size_t>(wave.frameCount) * wave.channels);
    UnloadWaveSamples(samples);
    UnloadWave(wave);
    if (!samples) throw std::runtime_error("Could not decode audio: " + file.string());
    return decoded;
}
