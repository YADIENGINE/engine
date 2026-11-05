#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>
#include "Audio/SoundOutput.h"

// The device, as a list of what it was told. A test can end a voice, or say that a file is playing.
class FakeSoundOutput : public SoundOutput {
public:
    struct Voice {
        SoundId id{};
        VoiceStart start{};
        float gain = 1.0f;
        float pan = 0.0f;
        float pitch = 1.0f;
        float volume = 1.0f;
        float muffle = 0.0f;
        std::optional<float> lastFadeSeconds{};
        bool paused = false;
        bool alive = true;
        std::optional<float> stoppedWithFade{};
    };

    [[nodiscard]] bool IsPlaying(SoundId id) const override { return playing.contains(id); }
    [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start) override {
        const VoiceId voice{++_lastVoice};
        voices[voice] = {.id = id,
                         .start = start,
                         .gain = start.gain,
                         .pan = start.pan,
                         .pitch = start.pitch,
                         .volume = start.volume,
                         .muffle = start.muffle,
                         .paused = start.paused};
        started.push_back(voice);
        return voice;
    }
    [[nodiscard]] bool IsVoiceAlive(VoiceId voice) const override {
        const auto found = voices.find(voice);
        return found != voices.end() && found->second.alive;
    }
    void SetVoicePlacement(VoiceId voice, float gain, float pan) override {
        voices.at(voice).gain = gain;
        voices.at(voice).pan = pan;
    }
    void SetVoicePitch(VoiceId voice, float pitch) override { voices.at(voice).pitch = pitch; }
    void FadeVoice(VoiceId voice, float volume, float seconds) override {
        voices.at(voice).volume = volume;
        voices.at(voice).lastFadeSeconds = seconds;
    }
    void MuffleVoice(VoiceId voice, float amount, float) override { voices.at(voice).muffle = amount; }
    void PauseVoice(VoiceId voice, bool paused) override { voices.at(voice).paused = paused; }
    void StopVoice(VoiceId voice, float fadeSeconds) override {
        voices.at(voice).alive = false;
        voices.at(voice).stoppedWithFade = fadeSeconds;
    }
    void FadeBus(SoundBus bus, float volume, float seconds) override { busFades[bus] = {volume, seconds}; }
    void MuffleBus(SoundBus bus, float amount, float seconds) override { busMuffles[bus] = {amount, seconds}; }
    [[nodiscard]] std::uint64_t ClockFrames() const override { return clock; }

    // Most recent first, as a test usually asks about the last one.
    [[nodiscard]] const Voice& Last() const { return voices.at(started.back()); }
    [[nodiscard]] Voice& Of(std::size_t order) { return voices.at(started.at(order)); }

    std::map<VoiceId, Voice> voices;
    std::vector<VoiceId> started;
    std::set<SoundId> playing;
    std::map<SoundBus, std::pair<float, float>> busFades;
    std::map<SoundBus, std::pair<float, float>> busMuffles;
    std::uint64_t clock = 0;

private:
    std::uint64_t _lastVoice = 0;
};
