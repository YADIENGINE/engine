#pragma once
#include <cmath>
#include <cstdint>

// What may light up the view at once: one start of each counts as one flash.
enum class FlashKind : std::uint8_t { Screen = 1, Light = 2 };

// For players sensitive to light, flashes start at most three times a second, on Update time. It is shared, or a
// screen flash and a light flash taking turns would double that. One asked for sooner does not happen: delaying it
// would put it out of step with its event.
class FlashLimit {
public:
    static constexpr float MinSecondsBetween = 1.0f / 3.0f;

    // Whether a flash of this kind may start now. A kind not used yet may join a flash begun since the last Update.
    [[nodiscard]] bool Start(FlashKind kind) {
        const auto bit = static_cast<std::uint8_t>(kind);
        if (_untilNext > 0.0f && (_startedNow == 0 || (_startedNow & bit) != 0)) return false;
        _untilNext = MinSecondsBetween;
        _startedNow |= bit;
        return true;
    }
    void Update(float deltaSeconds) {
        if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
        _untilNext = _untilNext > deltaSeconds ? _untilNext - deltaSeconds : 0.0f;
        _startedNow = 0;
    }
    void Clear() {
        _untilNext = 0.0f;
        _startedNow = 0;
    }

private:
    float _untilNext = 0.0f;
    std::uint8_t _startedNow = 0; // Kinds begun since the last Update
};
