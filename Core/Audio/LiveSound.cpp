#include "Audio/LiveSound.h"
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Audio/Internals/LiveVoices.h"
#include "Audio/Internals/Validation.h"

LiveSound::LiveSound(std::weak_ptr<LiveVoices> owner, VoiceId voice)
    : _owner(std::move(owner))
    , _voice(voice) {}

LiveSound::~LiveSound() { Stop(); }

LiveSound::LiveSound(LiveSound&& other) noexcept
    : _owner(std::move(other._owner))
    , _voice(std::exchange(other._voice, VoiceId{})) {}

LiveSound& LiveSound::operator=(LiveSound&& other) noexcept {
    if (this == &other) return *this;
    Stop();
    _owner = std::move(other._owner);
    _voice = std::exchange(other._voice, VoiceId{});
    return *this;
}

std::shared_ptr<LiveVoices> LiveSound::Owner() const {
    return _voice == VoiceId{} ? nullptr : _owner.lock();
}

bool LiveSound::Playing() const {
    const auto owner = Owner();
    return owner && owner->Alive(_voice);
}

void LiveSound::SetVolume(float volume, float seconds) {
    ValidateAudioVolume(volume);
    ValidateAudioFade(seconds);
    if (const auto owner = Owner()) owner->Output().FadeVoice(_voice, volume, seconds);
}

void LiveSound::SetPitch(float pitch) {
    ValidateAudioPitch(pitch);
    if (const auto owner = Owner()) owner->Output().SetVoicePitch(_voice, pitch);
}

void LiveSound::SetPosition(Float2 position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y))
        throw std::invalid_argument("A sound's position must be finite");
    if (const auto owner = Owner()) owner->SetPosition(_voice, position);
}

void LiveSound::SetPan(float pan) {
    ValidateAudioPan(pan);
    if (const auto owner = Owner()) owner->SetPan(_voice, pan);
}

void LiveSound::SetMuffle(float amount, float seconds) {
    ValidateAudioMuffle(amount);
    ValidateAudioFade(seconds);
    if (const auto owner = Owner()) owner->Output().MuffleVoice(_voice, amount, seconds);
}

void LiveSound::Stop(float fadeSeconds) {
    ValidateAudioFade(fadeSeconds);
    if (const auto owner = Owner()) owner->Stop(_voice, fadeSeconds);
    _voice = VoiceId{};
}
