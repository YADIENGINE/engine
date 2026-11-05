#pragma once
#include "Audio/SoundOutput.h"

// All that lies between SoundPlayer and the device. In the EngineAudio target; the player itself builds headlessly.
class AudioSoundOutput final : public SoundOutput {
public:
    explicit AudioSoundOutput(AudioSystem& audio)
        : _audio(audio) {}
    [[nodiscard]] bool IsPlaying(SoundId id) const override { return _audio.IsEffectPlaying(id); }
    [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start) override { return _audio.StartVoice(id, start); }
    [[nodiscard]] bool IsVoiceAlive(VoiceId voice) const override { return _audio.IsVoiceAlive(voice); }
    void SetVoicePlacement(VoiceId voice, float gain, float pan) override { _audio.SetVoicePlacement(voice, gain, pan); }
    void SetVoicePitch(VoiceId voice, float pitch) override { _audio.SetVoicePitch(voice, pitch); }
    void FadeVoice(VoiceId voice, float volume, float seconds) override { _audio.FadeVoice(voice, volume, seconds); }
    void MuffleVoice(VoiceId voice, float amount, float seconds) override { _audio.MuffleVoice(voice, amount, seconds); }
    void PauseVoice(VoiceId voice, bool paused) override { _audio.PauseVoice(voice, paused); }
    void StopVoice(VoiceId voice, float fadeSeconds) override { _audio.StopVoice(voice, fadeSeconds); }
    void FadeBus(SoundBus bus, float volume, float seconds) override { _audio.FadeBus(bus, volume, seconds); }
    void MuffleBus(SoundBus bus, float amount, float seconds) override { _audio.MuffleBus(bus, amount, seconds); }
    [[nodiscard]] std::uint64_t ClockFrames() const override { return _audio.ClockFrames(); }

private:
    AudioSystem& _audio;
};
