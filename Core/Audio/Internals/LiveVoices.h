#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
#include "Audio/SoundBus.h"
#include "Audio/SoundOutput.h"
#include "Audio/SoundSpace.h"

// The voices handed out as LiveSounds, and the listeners that place them. Owned by the player, and shared so that a
// handle finds out the player is gone instead of calling into it.
class LiveVoices {
public:
    explicit LiveVoices(SoundOutput& output)
        : _output(output) {}

    [[nodiscard]] SoundOutput& Output() { return _output; }

    void Add(VoiceId voice, SoundBus bus, const SoundFalloff& falloff, std::optional<Float2> at);
    [[nodiscard]] bool Alive(VoiceId voice) const;
    void SetPosition(VoiceId voice, Float2 position);
    void SetPan(VoiceId voice, float pan);
    void Stop(VoiceId voice, float fadeSeconds);

    void SetListeners(std::span<const SoundListener> listeners);
    [[nodiscard]] std::span<const SoundListener> Listeners() const { return _listeners; }
    [[nodiscard]] SoundPlacement Place(Float2 position, const SoundFalloff& falloff) const {
        return PlaceSound(_listeners, position, falloff);
    }

    void SetBusPaused(SoundBus bus, bool paused);
    [[nodiscard]] bool BusPaused(SoundBus bus) const { return _busPaused[Index(bus)]; }
    [[nodiscard]] bool Held(SoundBus bus) const { return _hold || _busPaused[Index(bus)]; }
    void SetHold(bool held);
    // Forgets the voices that have ended.
    void Sweep();
    [[nodiscard]] std::size_t Count() const { return _voices.size(); }

private:
    struct Voice {
        SoundBus bus = SoundBus::World;
        SoundFalloff falloff{};
        std::optional<Float2> at{};
    };
    [[nodiscard]] static constexpr std::size_t Index(SoundBus bus) { return static_cast<std::size_t>(bus); }
    void ApplyPause();

    SoundOutput& _output;
    std::unordered_map<VoiceId, Voice> _voices;
    std::vector<SoundListener> _listeners{SoundListener{}};
    std::array<bool, AllSoundBuses.size()> _busPaused{};
    bool _hold = false;
};
