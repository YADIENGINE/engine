#include "Audio/AudioSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include "Utilities/StartupTimer.h"

namespace {
void CheckVolume(float volume) {
    if (!(volume >= 0.0f && volume <= 1.0f)) {
        throw std::invalid_argument("Audio volume must be between 0 and 1");
    }
}
} // namespace

AudioSystem::AudioSystem(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings)
    : _root(std::filesystem::absolute(std::move(assetRoot)))
    , _volumePolicy(std::move(volumeSettings)) {
    if (IsAudioDeviceReady()) throw std::logic_error("The audio device is already owned");
}

AudioSystem::~AudioSystem() {
    if (_loading) {
        _cancelLoading.store(true);
        AdoptLoaded();
    }
    Release();
}

void AudioSystem::BeginLoading(AudioCatalog catalog) {
    if (_loading || IsAudioDeviceReady()) throw std::logic_error("The audio device is already open");
    _loader = std::thread([this, catalog = std::move(catalog)] { LoadInBackground(catalog); });
    _loading = true;
}

bool AudioSystem::FinishLoading() {
    if (!_loading) return true;
    if (!_loaderDone.load(std::memory_order_acquire)) return false;
    AdoptLoaded();
    if (_loadError) std::rethrow_exception(std::exchange(_loadError, nullptr));
    return true;
}

void AudioSystem::LoadInBackground(const AudioCatalog& catalog) {
    try {
        {
            const StartupTimer timer("Audio device");
            InitAudioDevice();
            if (!IsAudioDeviceReady()) throw std::runtime_error("Could not initialize the audio device");
        }
        {
            const StartupTimer timer("Sound effects (all)");
            for (const auto& effect : catalog.effects) {
                if (_cancelLoading.load()) break;
                const auto index = static_cast<std::size_t>(effect.id);
                if (index >= _loadedEffects.size()) _loadedEffects.resize(index + 1);
                Effect& slot = _loadedEffects[index];
                ReleaseEffect(slot); // Katalogda aynı yuva iki kez varsa sonuncusu kalır
                slot.voices = LoadVoices(effect.file, DefaultVoices);
            }
        }
        const StartupTimer timer("Music streams (all)");
        for (const auto& music : catalog.music) {
            if (_cancelLoading.load()) break;
            const auto index = static_cast<std::size_t>(music.id);
            if (index >= _loadedMusic.size()) _loadedMusic.resize(index + 1);
            MusicSlot& slot = _loadedMusic[index];
            if (IsMusicValid(slot.stream)) UnloadMusicStream(slot.stream);
            slot = {.stream = OpenMusic(music.file), .volumeGroup = music.volumeGroup};
        }
    } catch (...) {
        _loadError = std::current_exception();
    }
    _loaderDone.store(true, std::memory_order_release);
}

void AudioSystem::AdoptLoaded() {
    _loader.join();
    _loading = false;
    _effects = std::move(_loadedEffects);
    _music = std::move(_loadedMusic);
    if (!IsAudioDeviceReady()) return;
    ApplyMasterVolume();
    SetEffectVolume(_effectVolume);
    SetMusicVolume(_musicVolume);
    SetMusicVolume(_menuMusicVolume, MusicVolumeGroup::Menu);
}

void AudioSystem::RequireLoaded() const {
    if (_loading) throw std::logic_error("Audio is still loading");
}
bool AudioSystem::AdjustEffectVolume(int steps) {
    const int previous = EffectVolumePercent();
    SetEffectVolume(_volumePolicy.Step(_effectVolume, steps));
    return EffectVolumePercent() != previous;
}
bool AudioSystem::AdjustMusicVolume(int steps, MusicVolumeGroup group) {
    const int previous = MusicVolumePercent(group);
    SetMusicVolume(_volumePolicy.Step(MusicVolume(group), steps), group);
    return MusicVolumePercent(group) != previous;
}
int AudioSystem::EffectVolumePercent() const { return AudioVolumePolicy::Percent(_effectVolume); }
int AudioSystem::MusicVolumePercent(MusicVolumeGroup group) const {
    return AudioVolumePolicy::Percent(MusicVolume(group));
}
void AudioSystem::LoadVolumeSettings(const GameOptions& options) {
    SetEffectVolume(_volumePolicy.LoadEffects(options));
    SetMusicVolume(_volumePolicy.LoadMusic(options));
    SetMusicVolume(_volumePolicy.LoadMenuMusic(options), MusicVolumeGroup::Menu);
}
void AudioSystem::StoreVolumeSettings(GameOptions& options) const {
    _volumePolicy.Store(options, _effectVolume, _musicVolume, _menuMusicVolume);
}

void AudioSystem::Release() {
    StopMusic();
    for (MusicSlot& slot : _music) {
        ::Music& music = slot.stream;
        if (IsMusicValid(music)) {
            UnloadMusicStream(music);
        }
        music = {};
    }
    for (Effect& effect : _effects) {
        ReleaseEffect(effect);
    }
    if (IsAudioDeviceReady()) CloseAudioDevice();
}

std::filesystem::path AudioSystem::Resolve(const std::filesystem::path& file) const {
    if (file.empty() || file.has_root_path()) {
        throw std::invalid_argument("Audio file path must be relative to the asset root: " + file.string());
    }
    for (const auto& part : file) {
        if (part == "..") {
            throw std::invalid_argument("Audio file path cannot leave the asset root: " + file.string());
        }
    }
    const auto path = _root / file;
    if (!std::filesystem::is_regular_file(path)) {
        throw std::runtime_error("Audio file not found: " + path.string());
    }
    return path;
}

void AudioSystem::ReleaseEffect(Effect& effect) {
    for (const Sound voice : effect.voices) {
        StopSound(voice);
    }
    for (std::size_t i = effect.voices.size(); i > 1; --i) {
        UnloadSoundAlias(effect.voices[i - 1]);
    }
    if (!effect.voices.empty()) {
        UnloadSound(effect.voices.front());
    }
    effect.voices.clear();
    effect.nextVoice = 0;
}

void AudioSystem::LoadEffect(SoundId id, const std::filesystem::path& file, std::size_t maxConcurrent) {
    RequireLoaded();
    std::vector<Sound> voices = LoadVoices(file, maxConcurrent);
    const auto index = static_cast<std::size_t>(id);
    if (index >= _effects.size()) _effects.resize(index + 1);

    // Yeni dosya yüklendi; yuvadaki eski efekt bırakılır, efekte özel ses seviyesi korunur.
    Effect& effect = _effects[index];
    ReleaseEffect(effect);
    effect.voices = std::move(voices);
    ApplyEffectVolume(effect);
}

std::vector<Sound> AudioSystem::LoadVoices(const std::filesystem::path& file, std::size_t maxConcurrent) const {
    if (maxConcurrent == 0) {
        throw std::invalid_argument("Effect maxConcurrent must be at least 1");
    }
    const auto path = Resolve(file);
    Effect loaded;
    loaded.voices.reserve(maxConcurrent);
    const Sound source = LoadSound(path.string().c_str());
    if (!IsSoundValid(source)) {
        throw std::runtime_error("Could not load sound: " + path.string());
    }
    loaded.voices.push_back(source);
    try {
        for (std::size_t i = 1; i < maxConcurrent; ++i) {
            const Sound alias = LoadSoundAlias(source);
            if (!IsSoundValid(alias)) {
                throw std::runtime_error("Could not create sound voice: " + path.string());
            }
            loaded.voices.push_back(alias);
        }
    } catch (...) {
        ReleaseEffect(loaded);
        throw;
    }
    return std::move(loaded.voices);
}

void AudioSystem::UnloadEffect(SoundId id) { ReleaseEffect(GetEffect(id)); }

bool AudioSystem::HasEffect(SoundId id) const {
    const auto index = static_cast<std::size_t>(id);
    return index < _effects.size() && !_effects[index].voices.empty();
}

AudioSystem::Effect& AudioSystem::GetEffect(SoundId id) {
    Effect& effect = _effects.at(static_cast<std::size_t>(id));
    if (effect.voices.empty()) {
        throw std::out_of_range("Effect not loaded: " + std::to_string(static_cast<int>(id)));
    }
    return effect;
}

void AudioSystem::PlayEffect(SoundId id, float pitch) {
    Effect& effect = GetEffect(id);
    for (std::size_t offset = 0; offset < effect.voices.size(); ++offset) {
        const std::size_t index = (effect.nextVoice + offset) % effect.voices.size();
        if (!IsSoundPlaying(effect.voices[index])) {
            SetSoundPitch(effect.voices[index], pitch);
            PlaySound(effect.voices[index]);
            effect.nextVoice = (index + 1) % effect.voices.size();
            return;
        }
    }
    SetSoundPitch(effect.voices[effect.nextVoice], pitch);
    PlaySound(effect.voices[effect.nextVoice]);
    effect.nextVoice = (effect.nextVoice + 1) % effect.voices.size();
}

void AudioSystem::PlayEffectIfIdle(SoundId id, float pitch) {
    const Effect& effect = GetEffect(id);
    if (std::ranges::any_of(effect.voices, [](Sound voice) { return IsSoundPlaying(voice); })) return;
    PlayEffect(id, pitch);
}

void AudioSystem::StopEffect(SoundId id) {
    for (const Sound voice : GetEffect(id).voices) {
        StopSound(voice);
    }
}

void AudioSystem::SetEffectVolume(SoundId id, float volume) {
    CheckVolume(volume);
    Effect& effect = GetEffect(id);
    effect.volume = volume;
    ApplyEffectVolume(effect);
}

void AudioSystem::ApplyEffectVolume(Effect& effect) const {
    for (const Sound voice : effect.voices) {
        SetSoundVolume(voice, _effectVolume * effect.volume);
    }
}

void AudioSystem::LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group) {
    RequireLoaded();
    const ::Music loaded = OpenMusic(file);
    const auto index = static_cast<std::size_t>(id);
    if (index >= _music.size()) _music.resize(index + 1);
    if (HasMusic(id)) {
        UnloadMusic(id);
    }
    _music[index].volumeGroup = group;
    ::Music& music = _music[index].stream;
    music = loaded;
    ::SetMusicVolume(music, MusicVolume(group));
}

::Music AudioSystem::OpenMusic(const std::filesystem::path& file) const {
    const auto path = Resolve(file);
    const ::Music loaded = LoadMusicStream(path.string().c_str());
    if (!IsMusicValid(loaded)) {
        throw std::runtime_error("Could not load music: " + path.string());
    }
    return loaded;
}

void AudioSystem::UnloadMusic(MusicId id) {
    ::Music& music = GetMusic(id);
    if (_currentMusic == id) {
        StopMusic();
    }
    UnloadMusicStream(music);
    music = {};
}

bool AudioSystem::HasMusic(MusicId id) const {
    const auto index = static_cast<std::size_t>(id);
    return index < _music.size() && IsMusicValid(_music[index].stream);
}

::Music& AudioSystem::GetMusic(MusicId id) {
    ::Music& music = _music.at(static_cast<std::size_t>(id)).stream;
    if (!IsMusicValid(music)) {
        throw std::out_of_range("Music not loaded: " + std::to_string(static_cast<int>(id)));
    }
    return music;
}

void AudioSystem::PlayMusic(MusicId id, bool loop) {
    ::Music& music = GetMusic(id);
    StopMusic();
    _currentMusic = id;
    music.looping = loop;
    ::SetMusicVolume(music, MusicVolume(_music[static_cast<std::size_t>(id)].volumeGroup));
    _musicPitch = 1.0f;
    ::SetMusicPitch(music, _musicPitch);
    PlayMusicStream(music);
}

void AudioSystem::SetMusicPitch(float pitch) {
    _musicPitch = pitch;
    if (_currentMusic) {
        ::SetMusicPitch(GetMusic(*_currentMusic), _musicPitch);
    }
}

void AudioSystem::StopMusic() {
    if (_currentMusic) {
        StopMusicStream(GetMusic(*_currentMusic));
        _currentMusic.reset();
    }
}

void AudioSystem::PauseMusic() {
    if (_currentMusic) {
        PauseMusicStream(GetMusic(*_currentMusic));
    }
}

void AudioSystem::ResumeMusic() {
    if (_currentMusic) {
        ResumeMusicStream(GetMusic(*_currentMusic));
    }
}

bool AudioSystem::IsMusicPlaying() const {
    return _currentMusic && IsMusicStreamPlaying(_music[static_cast<std::size_t>(*_currentMusic)].stream);
}

void AudioSystem::Update() {
    if (_currentMusic) {
        UpdateMusicStream(GetMusic(*_currentMusic));
    }
}

void AudioSystem::ApplyMasterVolume() const {
    if (!_loading) ::SetMasterVolume(_muted ? 0.0f : _masterVolume); // Yüklenirken cihaz worker'ındır
}

void AudioSystem::SetMasterVolume(float volume) {
    CheckVolume(volume);
    _masterVolume = volume;
    ApplyMasterVolume();
}

void AudioSystem::SetMuted(bool muted) {
    _muted = muted;
    ApplyMasterVolume();
}

void AudioSystem::SetEffectVolume(float volume) {
    CheckVolume(volume);
    _effectVolume = volume;
    for (Effect& effect : _effects) {
        ApplyEffectVolume(effect);
    }
}

void AudioSystem::SetMusicVolume(float volume, MusicVolumeGroup group) {
    CheckVolume(volume);
    if (group == MusicVolumeGroup::Menu)
        _menuMusicVolume = volume;
    else
        _musicVolume = volume;
    for (const MusicSlot& slot : _music) {
        if (slot.volumeGroup == group && IsMusicValid(slot.stream)) {
            ::SetMusicVolume(slot.stream, volume);
        }
    }
}
