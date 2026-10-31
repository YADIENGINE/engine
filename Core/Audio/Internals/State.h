#pragma once
#include <atomic>
#include <exception>
#include <stop_token>
#include <thread>
#include <unordered_map>
#include "Audio/AudioSystem.h"
#include "Audio/Internals/AudioDevice.h"
#include "Audio/Internals/AudioMusicStream.h"
#include "Audio/Internals/AudioVolumePolicy.h"
#include "Audio/Internals/EffectVoices.h"

class AudioSystem::State final {
public:
    State(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings);
    ~State();
    void BeginLoading(AudioCatalog catalog);
    [[nodiscard]] bool FinishLoading();

    void LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings);
    void UnloadEffect(SoundId id);
    [[nodiscard]] bool HasEffect(SoundId id) const;
    void PlayEffect(SoundId id, float pitch, bool onlyIfIdle);
    void StopEffect(SoundId id);
    void SetEffectVolume(SoundId id, float volume);

    void LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group);
    void UnloadMusic(MusicId id);
    [[nodiscard]] bool HasMusic(MusicId id) const;
    void PlayMusic(MusicId id, bool loop);
    void SetMusicPitch(float pitch);
    [[nodiscard]] float MusicPitch() const { return _musicPitch; }
    [[nodiscard]] std::optional<MusicId> CurrentMusic() const { return _currentMusic; }
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();
    [[nodiscard]] bool IsMusicPlaying() const;
    void Update();

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

    void LoadInBackground(const AudioCatalog& catalog, std::stop_token stop);
    void RequireReady() const;
    [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path& file) const;
    [[nodiscard]] EffectVoices& GetEffect(SoundId id);
    [[nodiscard]] AudioMusicStream& GetMusic(MusicId id);
    void ApplyMasterVolume() const;

    // Declared first: all streams and voices are destroyed before the device closes.
    AudioDevice _device;
    std::filesystem::path _root;
    AudioVolumePolicy _volumePolicy;
    std::unordered_map<SoundId, EffectVoices> _effects;
    std::unordered_map<MusicId, AudioMusicStream> _music;
    std::optional<MusicId> _currentMusic;
    float _musicPitch = 1.0f;
    float _masterVolume = 1.0f;
    float _effectVolume = 1.0f;
    float _musicVolume = 1.0f;
    float _menuMusicVolume = 1.0f;
    bool _muted = false;
    LoadState _loadState = LoadState::NotStarted;

    // Only the worker accesses these buffers until FinishLoading joins it.
    std::atomic<bool> _loaderDone{false};
    std::exception_ptr _loadError;
    std::unordered_map<SoundId, EffectVoices> _loadedEffects;
    std::unordered_map<MusicId, AudioMusicStream> _loadedMusic;
    // Declared last: joins before any worker-owned state or resources are destroyed.
    std::jthread _loader;
};
