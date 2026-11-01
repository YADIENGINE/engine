#include "Audio/Internals/State.h"
#include <stdexcept>
#include <string>
#include <utility>
#include "Audio/Internals/Validation.h"
#include "Runtime/Internals/StartupTimer.h"

AudioSystem::State::State(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings)
    : _root(std::filesystem::absolute(std::move(assetRoot)))
    , _volumePolicy(volumeSettings) {}
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
        _loadedMusic.clear();
        _loadedEffects.clear();
        _loadState = LoadState::Failed;
        std::rethrow_exception(_loadError);
    }
    _effects = std::move(_loadedEffects);
    _music = std::move(_loadedMusic);
    _loadState = LoadState::Ready;
    ApplyMasterVolume();
    SetEffectVolume(_effectVolume);
    SetMusicVolume(_musicVolume);
    SetMusicVolume(_menuMusicVolume, MusicVolumeGroup::Menu);
    return true;
}
void AudioSystem::State::LoadInBackground(const AudioCatalog& catalog, std::stop_token stop) {
    try {
        {
            const StartupTimer timer("Audio device");
            _device.Open();
        }
        {
            const StartupTimer timer("Sound effects (all)");
            for (const auto& effect : catalog.effects) {
                if (stop.stop_requested()) break;
                _loadedEffects.emplace(effect.id, EffectVoices(Resolve(effect.file), effect.settings));
            }
        }
        const StartupTimer timer("Music streams (all)");
        for (const auto& music : catalog.music) {
            if (stop.stop_requested()) break;
            _loadedMusic.emplace(music.id, AudioMusicStream(Resolve(music.file), music.volumeGroup));
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

void AudioSystem::State::LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings) {
    RequireReady();
    ValidateAudioEffectSettings(settings);
    EffectVoices loaded(Resolve(file), settings);
    loaded.ApplyVolume(_effectVolume);
    _effects.insert_or_assign(id, std::move(loaded));
}
void AudioSystem::State::UnloadEffect(SoundId id) {
    (void)GetEffect(id);
    _effects.erase(id);
}
bool AudioSystem::State::HasEffect(SoundId id) const { return _loadState == LoadState::Ready && _effects.contains(id); }
EffectVoices& AudioSystem::State::GetEffect(SoundId id) {
    RequireReady();
    const auto found = _effects.find(id);
    if (found == _effects.end())
        throw std::out_of_range("Effect not loaded: " + std::to_string(static_cast<std::size_t>(id)));
    return found->second;
}
void AudioSystem::State::PlayEffect(SoundId id, float pitch, bool onlyIfIdle) {
    EffectVoices& effect = GetEffect(id);
    ValidateAudioPitch(pitch);
    effect.Play(pitch, onlyIfIdle);
}
void AudioSystem::State::StopEffect(SoundId id) { GetEffect(id).Stop(); }
void AudioSystem::State::SetEffectVolume(SoundId id, float volume) {
    EffectVoices& effect = GetEffect(id);
    ValidateAudioVolume(volume);
    effect.SetVolume(volume);
    effect.ApplyVolume(_effectVolume);
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
    AudioMusicStream loaded(path, group);
    ::SetMusicVolume(loaded.Stream(), MusicVolume(group));
    if (_currentMusic == id) StopMusic();
    _music.insert_or_assign(id, std::move(loaded));
}
void AudioSystem::State::UnloadMusic(MusicId id) {
    (void)GetMusic(id);
    if (_currentMusic == id) StopMusic();
    _music.erase(id);
}
bool AudioSystem::State::HasMusic(MusicId id) const { return _loadState == LoadState::Ready && _music.contains(id); }
AudioMusicStream& AudioSystem::State::GetMusic(MusicId id) {
    RequireReady();
    const auto found = _music.find(id);
    if (found == _music.end())
        throw std::out_of_range("Music not loaded: " + std::to_string(static_cast<std::size_t>(id)));
    return found->second;
}
void AudioSystem::State::PlayMusic(MusicId id, bool loop) {
    AudioMusicStream& slot = GetMusic(id);
    StopMusic();
    ::Music& music = slot.Stream();
    music.looping = loop;
    ::SetMusicVolume(music, MusicVolume(slot.Group()));
    _musicPitch = 1.0f;
    ::SetMusicPitch(music, _musicPitch);
    PlayMusicStream(music);
    _currentMusic = id;
}
void AudioSystem::State::SetMusicPitch(float pitch) {
    RequireReady();
    ValidateAudioPitch(pitch);
    if (_currentMusic) ::SetMusicPitch(GetMusic(*_currentMusic).Stream(), pitch);
    _musicPitch = pitch;
}
void AudioSystem::State::StopMusic() {
    if (!_currentMusic) return;
    StopMusicStream(GetMusic(*_currentMusic).Stream());
    _currentMusic.reset();
}
void AudioSystem::State::PauseMusic() {
    if (_currentMusic) PauseMusicStream(GetMusic(*_currentMusic).Stream());
}
void AudioSystem::State::ResumeMusic() {
    if (_currentMusic) ResumeMusicStream(GetMusic(*_currentMusic).Stream());
}
bool AudioSystem::State::IsMusicPlaying() const {
    return _currentMusic && IsMusicStreamPlaying(_music.at(*_currentMusic).Stream());
}
float AudioSystem::State::MusicLength() const {
    return _currentMusic ? GetMusicTimeLength(_music.at(*_currentMusic).Stream()) : 0.0f;
}
float AudioSystem::State::MusicPosition() const {
    return _currentMusic ? GetMusicTimePlayed(_music.at(*_currentMusic).Stream()) : 0.0f;
}
void AudioSystem::State::SeekMusic(float seconds) {
    ValidateAudioSeek(seconds);
    if (!_currentMusic) return;
    ::Music& music = GetMusic(*_currentMusic).Stream();
    SeekMusicStream(music, AudioSeekPosition(seconds, GetMusicTimeLength(music)));
}
void AudioSystem::State::SetMusicLooping(bool loop) {
    if (_currentMusic) GetMusic(*_currentMusic).Stream().looping = loop;
}
void AudioSystem::State::Update() {
    if (_currentMusic) UpdateMusicStream(GetMusic(*_currentMusic).Stream());
}

void AudioSystem::State::ApplyMasterVolume() const {
    if (_loadState == LoadState::Ready) ::SetMasterVolume(_muted ? 0.0f : _masterVolume);
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
    for (auto& [id, effect] : _effects) {
        (void)id;
        effect.ApplyVolume(_effectVolume);
    }
}
void AudioSystem::State::SetMusicVolume(float volume, MusicVolumeGroup group) {
    ValidateAudioVolume(volume);
    ValidateAudioMusicGroup(group);
    if (group == MusicVolumeGroup::Menu)
        _menuMusicVolume = volume;
    else
        _musicVolume = volume;
    for (auto& [id, slot] : _music) {
        (void)id;
        if (slot.Group() == group) ::SetMusicVolume(slot.Stream(), volume);
    }
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
