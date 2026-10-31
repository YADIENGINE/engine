#pragma once

// Game time flow: hitstop (brief freeze) and slow motion. Game passes each frame's real duration through this;
// the game and effects advance by the returned duration.
class TimeScale {
public:
    // 'seconds' boyunca zaman durur.
    void Hitstop(float seconds);
    // Transitions from startScale to endScale over 'seconds' real seconds, then returns to normal.
    // Equal scales give constant slow motion; without endScale it gradually returns to normal.
    void SlowMo(float seconds, float startScale, float endScale = 1.0f);
    void Clear();
    // Converts real duration to game duration; 0 during hitstop.
    [[nodiscard]] float Advance(float realSeconds);

private:
    float _hitstopSeconds = 0.0f; // Remaining freeze duration
    float _slowMoSeconds = 0.0f;  // Remaining slow-motion duration
    float _slowMoDuration = 0.0f;
    float _slowMoStartScale = 1.0f;
    float _slowMoEndScale = 1.0f;
};
