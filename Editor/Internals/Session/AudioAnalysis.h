#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "Editor/Internals/Session/AudioPreview.h"

// Numbers about a decoded sound that a tool or agent can read instead of listening: levels in dBFS (0 is full scale),
// silent edges and clipped samples.
struct AudioAnalysis {
    float seconds = 0.0f;
    int sampleRate = 0;
    int channels = 0;
    std::size_t frames = 0;
    float peakDb = 0.0f;
    float rmsDb = 0.0f;
    float leadingSilence = 0.0f;  // Seconds before the first frame louder than the silence level
    float trailingSilence = 0.0f; // Seconds after the last one
    std::size_t clipped = 0;      // Samples at or beyond full scale
};
inline constexpr float AudioFloorDb = -120.0f; // Silence reports this instead of minus infinity
inline constexpr float DefaultSilenceDb = -60.0f;

[[nodiscard]] float AudioDecibels(float amplitude);
[[nodiscard]] AudioAnalysis AnalyzeAudio(std::span<const float> samples, int channels, int sampleRate,
                                         float silenceDb = DefaultSilenceDb);

// A waveform drawn as RGBA pixels, one summary bucket per column (summarize with `width` buckets). Faint vertical
// lines mark every `tickSeconds`.
struct WaveformImage {
    int width = 0;
    int height = 0;
    float tickSeconds = 0.0f;
    std::vector<std::uint8_t> rgba;
};
// The tick interval that leaves at most about twenty ticks across `seconds`.
[[nodiscard]] float WaveformTickSeconds(float seconds);
[[nodiscard]] WaveformImage DrawWaveformImage(const AudioWaveform& waveform, int width, int height);
