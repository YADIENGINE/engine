#pragma once
#include "Animation/Curve.h"
#include "Animation/Tween.h"

// Game time flow: hitstop (brief freeze) and slow motion. Game passes each frame's real duration through this;
// the game and effects advance by the returned duration. Invalid times and scales throw.
class TimeScale {
public:
    // Time stands still for 'seconds'; the longer of two requests wins.
    void Hitstop(float seconds);
    // Moves from startScale to endScale over 'seconds' real seconds, then returns to normal. Equal scales give constant
    // slow motion; without endScale it gradually returns to normal. 'curve' shapes the way.
    void SlowMo(float seconds, float startScale, float endScale = 1.0f, Curve curve = Curve::Linear);
    void Clear();
    // Converts real duration to game duration; 0 during hitstop.
    [[nodiscard]] float Advance(float realSeconds);

private:
    float _hitstopSeconds = 0.0f; // Remaining freeze duration
    SecondsTween<float> _slowMo{1.0f};
};
