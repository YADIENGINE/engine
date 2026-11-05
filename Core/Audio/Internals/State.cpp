#include "Audio/Internals/State.h"
#include <algorithm>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include "Audio/Internals/Validation.h"
#include "Runtime/Internals/StartupTimer.h"

namespace {
// A sound can reach √2 in one ear (PanEars), so the mix keeps this headroom. A sound in the middle at full volume comes
// out at 0.707: raylib's mix, which the engine used before, gave 0.6875 there, so games sound as they did.
constexpr float MixHeadroom = 1.0f / std::numbers::sqrt2_v<float>;
} // namespace

AudioSystem::State::State(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings, AudioOutput output)
    : _root(std::filesystem::absolute(std::move(assetRoot)))
    , _volumePolicy(volumeSettings)
    , _output(output) {
    if (output != AudioOutput::Device && output != AudioOutput::Offline)
        throw std::invalid_argument("Invalid audio output");
}
AudioSystem::State::~State() { _loader.request_stop(); }

void AudioSystem::State::BeginLoading(AudioCatalog catalog) {
    if (_loadState != LoadState::NotStarted) throw std::logic_error("Audio loading has already been started");
    AudioSystem::ValidateCatalog(catalog);
    _loader =
        std::jthread([this, catalog = std::move(catalog)](std::stop_token stop) { LoadInBackground(catalog, stop); });
    _loadState = LoadState::Loading;
}
bool AudioSystem::State::FinishLoading() {
    if (_loadState == LoadState::NotStarted) throw std::logic_error("Audio loading has not been started");
    if (_loadState == LoadState::Ready) return true;
    if (_loadState == LoadState::Failed) std::rethrow_exception(_loadError);
    if (!_loaderDone.load(std::memory_order_acquire)) return false;
    _loader.join();
    if (_loadError) {
        _voices.clear();
        _music.clear();
        _effects.clear();
        _graph.reset();
        _loadState = LoadState::Failed;
        std::rethrow_exception(_loadError);
    }
    _loadState = LoadState::Ready;
    SnapVolumes();
    return true;
}
void AudioSystem::State::LoadInBackground(const AudioCatalog& catalog, std::stop_token stop) {
    try {
        {
            const StartupTimer timer("Audio device");
            _graph = std::make_unique<AudioGraph>(_output);
        }
        {
            const StartupTimer timer("Sound effects (all)");
            for (const auto& effect : catalog.effects) {
                if (stop.stop_requested()) break;
                _effects.emplace(effect.id, MakeEffect(Resolve(effect.file), effect.settings));
            }
        }
        const StartupTimer timer("Music streams (all)");
        for (const auto& music : catalog.music) {
            if (stop.stop_requested()) break;
            _music.emplace(music.id, MakeMusic(Resolve(music.file), music.volumeGroup));
        }
    } catch (...) {
        _loadError = std::current_exception();
    }
    _loaderDone.store(true, std::memory_order_release);
}
void AudioSystem::State::RequireReady() const {
    if (_loadState == LoadState::Failed) std::rethrow_exception(_loadError);
    if (_loadState != LoadState::Ready)
        throw std::logic_error(
            "Audio is not ready; complete BeginLoading/FinishLoading before playback or resource changes");
}
std::filesystem::path AudioSystem::State::Resolve(const std::filesystem::path& file) const {
    ValidateAudioPath(file);
    const auto path = _root / file;
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Audio file not found: " + path.string());
    return path;
}

AudioSystem::State::Effect AudioSystem::State::MakeEffect(const std::filesystem::path& path,
                                                          const EffectSettings& settings) {
    Effect effect{path, settings, {}};
    // The first voice proves the file loads, and holds a decoded file in memory for the voices after it.
    effect.voices.push_back(
        std::make_unique<AudioVoice>(_graph->Engine(), _graph->Input(settings.bus), path, settings.streamed));
    return effect;
}

void AudioSystem::State::ForgetVoices(Effect& effect) {
    for (const auto& voice : effect.voices)
        if (voice->Busy()) _voices.erase(voice->Id());
}

void AudioSystem::State::LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings) {
    RequireReady();
    ValidateAudioEffectSettings(settings);
    Effect loaded = MakeEffect(Resolve(file), settings);
    if (const auto old = _effects.find(id); old != _effects.end()) ForgetVoices(old->second);
    _effects.insert_or_assign(id, std::move(loaded));
}
void AudioSystem::State::UnloadEffect(SoundId id) {
    ForgetVoices(GetEffect(id));
    _effects.erase(id);
}
bool AudioSystem::State::HasEffect(SoundId id) const { return _loadState == LoadState::Ready && _effects.contains(id); }
AudioSystem::State::Effect& AudioSystem::State::GetEffect(SoundId id) {
    RequireReady();
    const auto found = _effects.find(id);
    if (found == _effects.end())
        throw std::out_of_range("Effect not loaded: " + std::to_string(static_cast<std::size_t>(id)));
    return found->second;
}
void AudioSystem::State::PlayEffect(SoundId id, float pitch, bool onlyIfIdle) {
    (void)GetEffect(id);
    ValidateAudioPitch(pitch);
    if (onlyIfIdle && IsEffectPlaying(id)) return;
    (void)StartVoice(id, {.pitch = pitch});
}
void AudioSystem::State::StopEffect(SoundId id) {
    for (const auto& voice : GetEffect(id).voices)
        voice->Stop(0.0f);
}
bool AudioSystem::State::IsEffectPlaying(SoundId id) const {
    if (_loadState != LoadState::Ready) return false;
    const auto found = _effects.find(id);
    if (found == _effects.end()) return false;
    return std::ranges::any_of(found->second.voices, [](const auto& voice) { return voice->Alive(); });
}
void AudioSystem::State::SetEffectVolume(SoundId id, float volume) {
    Effect& effect = GetEffect(id);
    ValidateAudioVolume(volume);
    effect.settings.volume = volume;
    for (const auto& voice : effect.voices)
        voice->SetBaseVolume(volume);
}

AudioVoice& AudioSystem::State::FreeVoice(Effect& effect) {
    for (const auto& voice : effect.voices) {
        voice->Tick();
        if (voice->Busy()) continue;
        return *voice;
    }
    effect.voices.push_back(std::make_unique<AudioVoice>(_graph->Engine(), _graph->Input(effect.settings.bus),
                                                         effect.file, effect.settings.streamed));
    return *effect.voices.back();
}

VoiceId AudioSystem::State::NextVoiceId() { return VoiceId{++_lastVoiceId}; }

VoiceId AudioSystem::State::StartVoice(SoundId id, const VoiceStart& start) {
    Effect& effect = GetEffect(id);
    ValidateAudioVoiceStart(start);
    if (!start.owned) {
        std::size_t playing = 0;
        AudioVoice* oldest = nullptr;
        for (const auto& voice : effect.voices) {
            voice->Tick();
            if (!voice->Alive() || voice->Owned()) continue;
            ++playing;
            if (!oldest || voice->Id() < oldest->Id()) oldest = voice.get();
        }
        if (playing >= effect.settings.maxConcurrent) {
            if (effect.settings.overflow == EffectOverflow::Skip) return VoiceId{};
            oldest->Stop(0.0f); // Fades in a few milliseconds beside the new one, where a restart would click
        }
    }
    AudioVoice& voice = FreeVoice(effect);
    const VoiceId started = NextVoiceId();
    voice.Begin(started, start, effect.settings.volume);
    _voices[started] = &voice;
    return started;
}

AudioVoice* AudioSystem::State::FindVoice(VoiceId voice) const {
    const auto found = _voices.find(voice);
    return found == _voices.end() ? nullptr : found->second;
}
bool AudioSystem::State::IsVoiceAlive(VoiceId voice) const {
    const AudioVoice* found = FindVoice(voice);
    return found && found->Alive();
}
void AudioSystem::State::SetVoicePlacement(VoiceId voice, float gain, float pan) {
    ValidateAudioVolume(gain);
    ValidateAudioPan(pan);
    if (AudioVoice* found = FindVoice(voice)) found->SetPlacement(gain, pan);
}
void AudioSystem::State::SetVoicePitch(VoiceId voice, float pitch) {
    ValidateAudioPitch(pitch);
    if (AudioVoice* found = FindVoice(voice)) found->SetPitch(pitch);
}
void AudioSystem::State::FadeVoice(VoiceId voice, float volume, float seconds) {
    ValidateAudioVolume(volume);
    ValidateAudioFade(seconds);
    if (AudioVoice* found = FindVoice(voice); found && found->Alive()) found->Fade(volume, seconds);
}
void AudioSystem::State::MuffleVoice(VoiceId voice, float amount, float seconds) {
    ValidateAudioMuffle(amount);
    ValidateAudioFade(seconds);
    if (AudioVoice* found = FindVoice(voice)) found->Muffle(amount, seconds);
}
void AudioSystem::State::PauseVoice(VoiceId voice, bool paused) {
    if (AudioVoice* found = FindVoice(voice)) found->Pause(paused);
}
void AudioSystem::State::StopVoice(VoiceId voice, float fadeSeconds) {
    ValidateAudioFade(fadeSeconds);
    if (AudioVoice* found = FindVoice(voice)) found->Stop(fadeSeconds);
}
void AudioSystem::State::FadeBus(SoundBus bus, float volume, float seconds) {
    RequireReady();
    ValidateAudioBus(bus);
    ValidateAudioVolume(volume);
    ValidateAudioFade(seconds);
    _graph->Bus(bus).Shaper().FadeTo(volume, seconds);
}
void AudioSystem::State::MuffleBus(SoundBus bus, float amount, float seconds) {
    RequireReady();
    ValidateAudioBus(bus);
    ValidateAudioMuffle(amount);
    ValidateAudioFade(seconds);
    _graph->Bus(bus).Shaper().MuffleTo(amount, seconds);
}
std::uint64_t AudioSystem::State::ClockFrames() const {
    return _loadState == LoadState::Ready ? _graph->ClockFrames() : 0;
}
int AudioSystem::State::SampleRate() const { return _loadState == LoadState::Ready ? _graph->SampleRate() : 0; }

AudioSystem::State::Music AudioSystem::State::MakeMusic(const std::filesystem::path& path, MusicVolumeGroup group) {
    return {std::make_unique<AudioVoice>(_graph->Engine(), _graph->MusicOption(group).Node(), path, true), group};
}
void AudioSystem::State::LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group) {
    RequireReady();
    ValidateAudioMusicGroup(group);
    StoreMusic(id, Resolve(file), group);
}
void AudioSystem::State::LoadMusicFile(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group) {
    ValidateAudioFilePath(file);
    RequireReady();
    ValidateAudioMusicGroup(group);
    if (!std::filesystem::is_regular_file(file)) throw std::runtime_error("Audio file not found: " + file.string());
    StoreMusic(id, file, group);
}
void AudioSystem::State::StoreMusic(MusicId id, const std::filesystem::path& path, MusicVolumeGroup group) {
    Music loaded = MakeMusic(path, group);
    if (_currentMusic == id) StopMusic(0.0f);
    _music.insert_or_assign(id, std::move(loaded));
}
void AudioSystem::State::UnloadMusic(MusicId id) {
    (void)GetMusic(id);
    if (_currentMusic == id) StopMusic(0.0f);
    _music.erase(id);
}
bool AudioSystem::State::HasMusic(MusicId id) const { return _loadState == LoadState::Ready && _music.contains(id); }
AudioSystem::State::Music& AudioSystem::State::GetMusic(MusicId id) {
    RequireReady();
    const auto found = _music.find(id);
    if (found == _music.end())
        throw std::out_of_range("Music not loaded: " + std::to_string(static_cast<std::size_t>(id)));
    return found->second;
}
void AudioSystem::State::PlayMusic(MusicId id, const MusicPlay& play) {
    AudioVoice& voice = *GetMusic(id).voice;
    ValidateAudioFade(play.fadeSeconds);
    if (_currentMusic != id) StopMusic(play.fadeSeconds);
    _musicPitch = 1.0f;
    voice.Begin(NextVoiceId(), {.pitch = _musicPitch, .fadeIn = play.fadeSeconds, .loop = play.loop, .owned = true}, 1.0f);
    _currentMusic = id;
}
void AudioSystem::State::SetMusicPitch(float pitch) {
    RequireReady();
    ValidateAudioPitch(pitch);
    if (_currentMusic) GetMusic(*_currentMusic).voice->SetPitch(pitch);
    _musicPitch = pitch;
}
void AudioSystem::State::StopMusic(float fadeSeconds) {
    ValidateAudioFade(fadeSeconds);
    if (!_currentMusic) return;
    GetMusic(*_currentMusic).voice->Stop(fadeSeconds);
    _currentMusic.reset();
}
void AudioSystem::State::PauseMusic() {
    if (_currentMusic) GetMusic(*_currentMusic).voice->Pause(true);
}
void AudioSystem::State::ResumeMusic() {
    if (_currentMusic) GetMusic(*_currentMusic).voice->Pause(false);
}
bool AudioSystem::State::IsMusicPlaying() const {
    if (!_currentMusic) return false;
    const AudioVoice& voice = *_music.at(*_currentMusic).voice;
    return voice.Alive() && !voice.Paused();
}
float AudioSystem::State::MusicLength() const { return _currentMusic ? _music.at(*_currentMusic).voice->Length() : 0.0f; }
float AudioSystem::State::MusicPosition() const {
    return _currentMusic ? _music.at(*_currentMusic).voice->Cursor() : 0.0f;
}
void AudioSystem::State::SeekMusic(float seconds) {
    ValidateAudioSeek(seconds);
    if (!_currentMusic) return;
    AudioVoice& voice = *GetMusic(*_currentMusic).voice;
    voice.Seek(AudioSeekPosition(seconds, voice.Length()));
}
void AudioSystem::State::SetMusicLooping(bool loop) {
    if (_currentMusic) GetMusic(*_currentMusic).voice->SetLooping(loop);
}
void AudioSystem::State::Update() {
    if (_loadState != LoadState::Ready) return;
    std::erase_if(_voices, [](const auto& entry) {
        entry.second->Tick();
        return entry.second->Id() != entry.first;
    });
    for (const auto& [id, music] : _music) {
        (void)id;
        music.voice->Tick();
    }
    _graph->DrainCapture();
}

void AudioSystem::State::Render(std::span<float> stereo) {
    RequireReady();
    _graph->Render(stereo);
}
void AudioSystem::State::StartCapture(const std::filesystem::path& wavFile, float seconds) {
    RequireReady();
    _graph->StartCapture(wavFile, seconds);
}

float AudioSystem::State::MasterLevel() const { return (_muted ? 0.0f : _masterVolume) * MixHeadroom; }
void AudioSystem::State::ApplyMasterVolume() {
    if (_loadState != LoadState::Ready) return;
    _graph->Master().Shaper().SetEars(MasterLevel(), MasterLevel());
}
void AudioSystem::State::SnapVolumes() {
    const auto snap = [](ShapeNode& node, float level) { node.Shaper().Snap({.left = level, .right = level}); };
    snap(_graph->Master(), MasterLevel());
    snap(_graph->Bus(SoundBus::World), _effectVolume);
    snap(_graph->Bus(SoundBus::Ui), _effectVolume);
    snap(_graph->MusicOption(MusicVolumeGroup::Default), _musicVolume);
    snap(_graph->MusicOption(MusicVolumeGroup::Menu), _menuMusicVolume);
}
void AudioSystem::State::ApplyBusVolumes() {
    if (_loadState != LoadState::Ready) return;
    _graph->Bus(SoundBus::World).Shaper().SetEars(_effectVolume, _effectVolume);
    _graph->Bus(SoundBus::Ui).Shaper().SetEars(_effectVolume, _effectVolume);
    _graph->MusicOption(MusicVolumeGroup::Default).Shaper().SetEars(_musicVolume, _musicVolume);
    _graph->MusicOption(MusicVolumeGroup::Menu).Shaper().SetEars(_menuMusicVolume, _menuMusicVolume);
}
void AudioSystem::State::SetMasterVolume(float volume) {
    ValidateAudioVolume(volume);
    _masterVolume = volume;
    ApplyMasterVolume();
}
void AudioSystem::State::SetMuted(bool muted) {
    _muted = muted;
    ApplyMasterVolume();
}
void AudioSystem::State::SetEffectVolume(float volume) {
    ValidateAudioVolume(volume);
    _effectVolume = volume;
    ApplyBusVolumes();
}
void AudioSystem::State::SetMusicVolume(float volume, MusicVolumeGroup group) {
    ValidateAudioVolume(volume);
    ValidateAudioMusicGroup(group);
    if (group == MusicVolumeGroup::Menu)
        _menuMusicVolume = volume;
    else
        _musicVolume = volume;
    ApplyBusVolumes();
}
float AudioSystem::State::MusicVolume(MusicVolumeGroup group) const {
    ValidateAudioMusicGroup(group);
    return group == MusicVolumeGroup::Menu ? _menuMusicVolume : _musicVolume;
}
bool AudioSystem::State::AdjustEffectVolume(int steps) {
    const int previous = EffectVolumePercent();
    SetEffectVolume(_volumePolicy.Step(_effectVolume, steps));
    return EffectVolumePercent() != previous;
}
bool AudioSystem::State::AdjustMusicVolume(int steps, MusicVolumeGroup group) {
    const int previous = MusicVolumePercent(group);
    SetMusicVolume(_volumePolicy.Step(MusicVolume(group), steps), group);
    return MusicVolumePercent(group) != previous;
}
int AudioSystem::State::EffectVolumePercent() const { return AudioVolumePolicy::Percent(_effectVolume); }
int AudioSystem::State::MusicVolumePercent(MusicVolumeGroup group) const {
    return AudioVolumePolicy::Percent(MusicVolume(group));
}
void AudioSystem::State::LoadVolumeSettings(const GameOptions& options) {
    SetEffectVolume(_volumePolicy.LoadEffects(options));
    SetMusicVolume(_volumePolicy.LoadMusic(options));
    SetMusicVolume(_volumePolicy.LoadMenuMusic(options), MusicVolumeGroup::Menu);
}
void AudioSystem::State::StoreVolumeSettings(GameOptions& options) const {
    _volumePolicy.Store(options, _effectVolume, _musicVolume, _menuMusicVolume);
}
