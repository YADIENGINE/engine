#pragma once
#include <atomic>
#include <exception>
#include <memory>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include <vector>
#include "Audio/AudioSystem.h"
#include "Audio/Internals/AudioGraph.h"
#include "Audio/Internals/AudioVoice.h"
#include "Audio/Internals/AudioVolumePolicy.h"

class AudioSystem::State final {
public:
    State(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings, AudioOutput output);
    ~State();
    void BeginLoading(AudioCatalog catalog);
    [[nodiscard]] bool FinishLoading();

    void LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings);
    void UnloadEffect(SoundId id);
    [[nodiscard]] bool HasEffect(SoundId id) const;
    void PlayEffect(SoundId id, float pitch, bool onlyIfIdle);
    void StopEffect(SoundId id);
    [[nodiscard]] bool IsEffectPlaying(SoundId id) const;
    void SetEffectVolume(SoundId id, float volume);

    [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start);
    [[nodiscard]] bool IsVoiceAlive(VoiceId voice) const;
    void SetVoicePlacement(VoiceId voice, float gain, float pan);
    void SetVoicePitch(VoiceId voice, float pitch);
    void FadeVoice(VoiceId voice, float volume, float seconds);
    void MuffleVoice(VoiceId voice, float amount, float seconds);
    void PauseVoice(VoiceId voice, bool paused);
    void StopVoice(VoiceId voice, float fadeSeconds);
    void FadeBus(SoundBus bus, float volume, float seconds);
    void MuffleBus(SoundBus bus, float amount, float seconds);
    [[nodiscard]] std::uint64_t ClockFrames() const;
    [[nodiscard]] int SampleRate() const;

    void LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group);
    void LoadMusicFile(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group);
    void UnloadMusic(MusicId id);
    [[nodiscard]] bool HasMusic(MusicId id) const;
    void PlayMusic(MusicId id, const MusicPlay& play);
    void SetMusicPitch(float pitch);
    [[nodiscard]] float MusicPitch() const { return _musicPitch; }
    [[nodiscard]] std::optional<MusicId> CurrentMusic() const { return _currentMusic; }
    void StopMusic(float fadeSeconds);
    void PauseMusic();
    void ResumeMusic();
    [[nodiscard]] bool IsMusicPlaying() const;
    [[nodiscard]] float MusicLength() const;
    [[nodiscard]] float MusicPosition() const;
    void SeekMusic(float seconds);
    void SetMusicLooping(bool loop);
    void Update();

    void Render(std::span<float> stereo);
    void StartCapture(const std::filesystem::path& wavFile, float seconds);
    [[nodiscard]] bool IsCapturing() const { return _graph && _graph->IsCapturing(); }

    void SetMasterVolume(float volume);
    void SetEffectVolume(float volume);
    void SetMusicVolume(float volume, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void SetMuted(bool muted);
    bool AdjustEffectVolume(int steps);
    bool AdjustMusicVolume(int steps, MusicVolumeGroup group);
    [[nodiscard]] int EffectVolumePercent() const;
    [[nodiscard]] int MusicVolumePercent(MusicVolumeGroup group) const;
    [[nodiscard]] float MasterVolume() const { return _masterVolume; }
    [[nodiscard]] float EffectVolume() const { return _effectVolume; }
    [[nodiscard]] float MusicVolume(MusicVolumeGroup group) const;
    [[nodiscard]] bool IsMuted() const { return _muted; }
    void LoadVolumeSettings(const GameOptions& options);
    void StoreVolumeSettings(GameOptions& options) const;

private:
    enum class LoadState { NotStarted, Loading, Ready, Failed };

    // One file of a catalog, with the voices made for it so far. Voices come when they are first needed and stay.
    struct Effect {
        std::filesystem::path file;
        EffectSettings settings;
        std::vector<std::unique_ptr<AudioVoice>> voices;
    };
    struct Music {
        std::unique_ptr<AudioVoice> voice;
        MusicVolumeGroup group = MusicVolumeGroup::Default;
    };

    void LoadInBackground(const AudioCatalog& catalog, std::stop_token stop);
    void RequireReady() const;
    [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path& file) const;
    [[nodiscard]] Effect MakeEffect(const std::filesystem::path& path, const EffectSettings& settings);
    [[nodiscard]] Music MakeMusic(const std::filesystem::path& path, MusicVolumeGroup group);
    void StoreMusic(MusicId id, const std::filesystem::path& path, MusicVolumeGroup group);
    [[nodiscard]] Effect& GetEffect(SoundId id);
    [[nodiscard]] Music& GetMusic(MusicId id);
    [[nodiscard]] AudioVoice& FreeVoice(Effect& effect);
    [[nodiscard]] AudioVoice* FindVoice(VoiceId voice) const;
    void ForgetVoices(Effect& effect);
    [[nodiscard]] float MasterLevel() const;
    void ApplyMasterVolume();
    void SnapVolumes();
    void ApplyBusVolumes();
    [[nodiscard]] VoiceId NextVoiceId();

    std::filesystem::path _root;
    AudioVolumePolicy _volumePolicy;
    AudioOutput _output;
    std::unique_ptr<AudioGraph> _graph; // Declared first of the audio: everything below goes before it does
    std::unordered_map<SoundId, Effect> _effects;
    std::unordered_map<MusicId, Music> _music;
    std::unordered_map<VoiceId, AudioVoice*> _voices; // Those handed out, until they end
    std::uint64_t _lastVoiceId = 0;
    std::optional<MusicId> _currentMusic;
    float _musicPitch = 1.0f;
    float _masterVolume = 1.0f;
    float _effectVolume = 1.0f;
    float _musicVolume = 1.0f;
    float _menuMusicVolume = 1.0f;
    bool _muted = false;
    LoadState _loadState = LoadState::NotStarted;

    // Until FinishLoading joins it, the worker alone touches the graph and the resources, and nothing reads them: every
    // public door checks _loadState first.
    std::atomic<bool> _loaderDone{false};
    std::exception_ptr _loadError;
    // Declared last: joins before any worker-owned state or resources are destroyed.
    std::jthread _loader;
};
