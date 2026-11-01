#include "Editor/Internals/Session/AudioAnalysis.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr float FullScale = 1.0f;
constexpr float MaxTicks = 20.0f;
constexpr std::array TickSteps{0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 15.0f, 30.0f, 60.0f, 120.0f, 300.0f};
using Pixel = std::array<std::uint8_t, 4>;
constexpr Pixel Background{24, 26, 30, 255};
constexpr Pixel Axis{70, 74, 82, 255};
constexpr Pixel Tick{44, 48, 55, 255};
constexpr Pixel Peak{70, 140, 95, 255};
constexpr Pixel Rms{120, 220, 150, 255};

float Seconds(std::size_t frames, int sampleRate) {
    return sampleRate > 0 ? static_cast<float>(frames) / static_cast<float>(sampleRate) : 0.0f;
}
} // namespace

float AudioDecibels(float amplitude) {
    return amplitude > 0.0f ? std::max(AudioFloorDb, 20.0f * std::log10(amplitude)) : AudioFloorDb;
}

AudioAnalysis AnalyzeAudio(std::span<const float> samples, int channels, int sampleRate, float silenceDb) {
    AudioAnalysis result;
    result.sampleRate = sampleRate;
    result.channels = channels;
    result.peakDb = result.rmsDb = AudioFloorDb;
    if (channels <= 0) return result;
    const auto width = static_cast<std::size_t>(channels);
    result.frames = samples.size() / width;
    result.seconds = Seconds(result.frames, sampleRate);
    if (result.frames == 0) return result;
    const float threshold = std::pow(10.0f, silenceDb / 20.0f);
    float peak = 0.0f;
    double sum = 0.0;
    std::size_t first = result.frames;
    std::size_t last = 0;
    for (std::size_t frame = 0; frame < result.frames; ++frame) {
        float loudest = 0.0f;
        for (std::size_t channel = 0; channel < width; ++channel) {
            const float value = std::abs(samples[frame * width + channel]);
            loudest = std::max(loudest, value);
            sum += static_cast<double>(value) * value;
            if (value >= FullScale) ++result.clipped;
        }
        peak = std::max(peak, loudest);
        if (loudest > threshold) {
            first = std::min(first, frame);
            last = frame;
        }
    }
    result.peakDb = AudioDecibels(peak);
    result.rmsDb = AudioDecibels(static_cast<float>(std::sqrt(sum / static_cast<double>(result.frames * width))));
    if (first == result.frames) { // Silent throughout
        result.leadingSilence = result.seconds;
        result.trailingSilence = 0.0f;
    } else {
        result.leadingSilence = Seconds(first, sampleRate);
        result.trailingSilence = Seconds(result.frames - 1 - last, sampleRate);
    }
    return result;
}

float WaveformTickSeconds(float seconds) {
    for (const float step : TickSteps)
        if (seconds / step <= MaxTicks) return step;
    return TickSteps.back();
}

WaveformImage DrawWaveformImage(const AudioWaveform& waveform, int width, int height) {
    WaveformImage image{std::max(width, 1), std::max(height, 1), WaveformTickSeconds(waveform.seconds), {}};
    image.rgba.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4);
    const auto put = [&](int x, int y, const Pixel& color) {
        if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
        std::ranges::copy(color, image.rgba.begin() + (static_cast<std::ptrdiff_t>(y) * image.width + x) * 4);
    };
    for (int y = 0; y < image.height; ++y)
        for (int x = 0; x < image.width; ++x)
            put(x, y, Background);
    if (waveform.seconds > 0.0f)
        for (float time = image.tickSeconds; time < waveform.seconds; time += image.tickSeconds) {
            const int x = static_cast<int>(time / waveform.seconds * static_cast<float>(image.width));
            for (int y = 0; y < image.height; ++y)
                put(x, y, Tick);
        }
    const float middle = static_cast<float>(image.height - 1) * 0.5f;
    for (int x = 0; x < image.width; ++x)
        put(x, static_cast<int>(middle), Axis);
    const auto count = waveform.peaks.size();
    for (int x = 0; x < image.width && count > 0; ++x) {
        const std::size_t bucket = static_cast<std::size_t>(x) * count / static_cast<std::size_t>(image.width);
        const auto column = [&](float amplitude, const Pixel& color) {
            const int reach = static_cast<int>(std::round(amplitude * middle));
            if (reach == 0) return; // Silence leaves the axis showing
            for (int y = static_cast<int>(middle) - reach; y <= static_cast<int>(std::ceil(middle)) + reach; ++y)
                put(x, y, color);
        };
        column(waveform.peaks[bucket], Peak);
        column(waveform.rms[bucket], Rms);
    }
    return image;
}
