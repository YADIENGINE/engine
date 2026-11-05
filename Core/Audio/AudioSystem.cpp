#include "Audio/AudioSystem.h"
#include <utility>
#include "Audio/Internals/State.h"

AudioSystem::AudioSystem(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings, AudioOutput output)
    : _state(std::make_unique<State>(std::move(assetRoot), volumeSettings, output)) {}
AudioSystem::~AudioSystem() = default;

void AudioSystem::BeginLoading(AudioCatalog catalog) { _state->BeginLoading(std::move(catalog)); }
bool AudioSystem::FinishLoading() { return _state->FinishLoading(); }

void AudioSystem::LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings) {
    _state->LoadEffect(id, file, settings);
}
void AudioSystem::UnloadEffect(SoundId id) { _state->UnloadEffect(id); }
bool AudioSystem::HasEffect(SoundId id) const { return _state->HasEffect(id); }
void AudioSystem::PlayEffect(SoundId id, float pitch) { _state->PlayEffect(id, pitch, false); }
void AudioSystem::PlayEffectIfIdle(SoundId id, float pitch) { _state->PlayEffect(id, pitch, true); }
void AudioSystem::StopEffect(SoundId id) { _state->StopEffect(id); }
bool AudioSystem::IsEffectPlaying(SoundId id) const { return _state->IsEffectPlaying(id); }
void AudioSystem::SetEffectVolume(SoundId id, float volume) { _state->SetEffectVolume(id, volume); }

VoiceId AudioSystem::StartVoice(SoundId id, const VoiceStart& start) { return _state->StartVoice(id, start); }
bool AudioSystem::IsVoiceAlive(VoiceId voice) const { return _state->IsVoiceAlive(voice); }
void AudioSystem::SetVoicePlacement(VoiceId voice, float gain, float pan) { _state->SetVoicePlacement(voice, gain, pan); }
void AudioSystem::SetVoicePitch(VoiceId voice, float pitch) { _state->SetVoicePitch(voice, pitch); }
void AudioSystem::FadeVoice(VoiceId voice, float volume, float seconds) { _state->FadeVoice(voice, volume, seconds); }
void AudioSystem::MuffleVoice(VoiceId voice, float amount, float seconds) { _state->MuffleVoice(voice, amount, seconds); }
void AudioSystem::PauseVoice(VoiceId voice, bool paused) { _state->PauseVoice(voice, paused); }
void AudioSystem::StopVoice(VoiceId voice, float fadeSeconds) { _state->StopVoice(voice, fadeSeconds); }
void AudioSystem::FadeBus(SoundBus bus, float volume, float seconds) { _state->FadeBus(bus, volume, seconds); }
void AudioSystem::MuffleBus(SoundBus bus, float amount, float seconds) { _state->MuffleBus(bus, amount, seconds); }
std::uint64_t AudioSystem::ClockFrames() const { return _state->ClockFrames(); }
int AudioSystem::SampleRate() const { return _state->SampleRate(); }

void AudioSystem::LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group) {
    _state->LoadMusic(id, file, group);
}
void AudioSystem::UnloadMusic(MusicId id) { _state->UnloadMusic(id); }
bool AudioSystem::HasMusic(MusicId id) const { return _state->HasMusic(id); }
void AudioSystem::PlayMusic(MusicId id, const MusicPlay& play) { _state->PlayMusic(id, play); }
void AudioSystem::SetMusicPitch(float pitch) { _state->SetMusicPitch(pitch); }
float AudioSystem::MusicPitch() const { return _state->MusicPitch(); }
std::optional<MusicId> AudioSystem::CurrentMusic() const { return _state->CurrentMusic(); }
void AudioSystem::StopMusic(float fadeSeconds) { _state->StopMusic(fadeSeconds); }
void AudioSystem::PauseMusic() { _state->PauseMusic(); }
void AudioSystem::ResumeMusic() { _state->ResumeMusic(); }
bool AudioSystem::IsMusicPlaying() const { return _state->IsMusicPlaying(); }
float AudioSystem::MusicLength() const { return _state->MusicLength(); }
float AudioSystem::MusicPosition() const { return _state->MusicPosition(); }
void AudioSystem::SeekMusic(float seconds) { _state->SeekMusic(seconds); }
void AudioSystem::SetMusicLooping(bool loop) { _state->SetMusicLooping(loop); }
void AudioSystem::Update() { _state->Update(); }
void AudioSystem::Render(std::span<float> stereo) { _state->Render(stereo); }
void AudioSystem::StartCapture(const std::filesystem::path& wavFile, float seconds) {
    _state->StartCapture(wavFile, seconds);
}
bool AudioSystem::IsCapturing() const { return _state->IsCapturing(); }

void AudioSystem::SetMasterVolume(float volume) { _state->SetMasterVolume(volume); }
void AudioSystem::SetEffectVolume(float volume) { _state->SetEffectVolume(volume); }
void AudioSystem::SetMusicVolume(float volume, MusicVolumeGroup group) { _state->SetMusicVolume(volume, group); }
void AudioSystem::SetMuted(bool muted) { _state->SetMuted(muted); }
bool AudioSystem::AdjustEffectVolume(int steps) { return _state->AdjustEffectVolume(steps); }
bool AudioSystem::AdjustMusicVolume(int steps, MusicVolumeGroup group) {
    return _state->AdjustMusicVolume(steps, group);
}
int AudioSystem::EffectVolumePercent() const { return _state->EffectVolumePercent(); }
int AudioSystem::MusicVolumePercent(MusicVolumeGroup group) const { return _state->MusicVolumePercent(group); }
float AudioSystem::MasterVolume() const { return _state->MasterVolume(); }
float AudioSystem::EffectVolume() const { return _state->EffectVolume(); }
float AudioSystem::MusicVolume(MusicVolumeGroup group) const { return _state->MusicVolume(group); }
bool AudioSystem::IsMuted() const { return _state->IsMuted(); }
void AudioSystem::LoadVolumeSettings(const GameOptions& options) { _state->LoadVolumeSettings(options); }
void AudioSystem::StoreVolumeSettings(GameOptions& options) const { _state->StoreVolumeSettings(options); }
