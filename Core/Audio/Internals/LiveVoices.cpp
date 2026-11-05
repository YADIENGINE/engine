#include "Audio/Internals/LiveVoices.h"
#include <algorithm>
#include <stdexcept>

void LiveVoices::Add(VoiceId voice, SoundBus bus, const SoundFalloff& falloff, std::optional<Float2> at) {
    _voices.emplace(voice, Voice{bus, falloff, at});
}

bool LiveVoices::Alive(VoiceId voice) const { return _voices.contains(voice) && _output.IsVoiceAlive(voice); }

void LiveVoices::SetPosition(VoiceId voice, Float2 position) {
    const auto found = _voices.find(voice);
    if (found == _voices.end()) return;
    found->second.at = position;
    const SoundPlacement placement = Place(position, found->second.falloff);
    _output.SetVoicePlacement(voice, placement.gain, placement.pan);
}

void LiveVoices::SetPan(VoiceId voice, float pan) {
    const auto found = _voices.find(voice);
    if (found == _voices.end()) return;
    found->second.at.reset();
    _output.SetVoicePlacement(voice, 1.0f, pan);
}

void LiveVoices::Stop(VoiceId voice, float fadeSeconds) {
    if (_voices.erase(voice) > 0) _output.StopVoice(voice, fadeSeconds);
}

void LiveVoices::SetListeners(std::span<const SoundListener> listeners) {
    for (const SoundListener& listener : listeners)
        if (!IsValid(listener)) throw std::invalid_argument("Invalid sound listener");
    _listeners.assign(listeners.begin(), listeners.end());
    for (const auto& [voice, state] : _voices) {
        if (!state.at) continue;
        const SoundPlacement placement = Place(*state.at, state.falloff);
        _output.SetVoicePlacement(voice, placement.gain, placement.pan);
    }
}

void LiveVoices::ApplyPause() {
    for (const auto& [voice, state] : _voices)
        _output.PauseVoice(voice, Held(state.bus));
}

void LiveVoices::SetBusPaused(SoundBus bus, bool paused) {
    if (_busPaused[Index(bus)] == paused) return;
    _busPaused[Index(bus)] = paused;
    ApplyPause();
}

void LiveVoices::SetHold(bool held) {
    if (_hold == held) return;
    _hold = held;
    ApplyPause();
}

void LiveVoices::Sweep() {
    std::erase_if(_voices, [this](const auto& entry) { return !_output.IsVoiceAlive(entry.first); });
}
