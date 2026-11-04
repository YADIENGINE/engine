#pragma once
#include <optional>
#include "Animation/Track.h"

// Timing of a scene transition: a curtain closes, holds, then opens. Update tells when it is fully closed, the moment
// to change scene. It draws nothing; give Cover to a curtain or to DrawIrisTransition.
class TransitionClock {
public:
    TransitionClock(float closeSeconds, float holdSeconds, float openSeconds);
    void Start();
    // True once per transition, in the call that reaches the fully closed curtain.
    [[nodiscard]] bool Update(float deltaSeconds);
    [[nodiscard]] bool Active() const { return _elapsed.has_value(); }
    // 0: open, 1: closed.
    [[nodiscard]] float Cover() const;
    // The curtain is on its way back.
    [[nodiscard]] bool Opening() const;

private:
    float _closeSeconds;
    float _openingAt;
    SecondsTrack<float> _cover;
    std::optional<float> _elapsed;
};
