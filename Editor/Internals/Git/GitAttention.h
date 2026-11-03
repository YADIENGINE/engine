#pragma once

// When Pull pulses. New commits pulse briefly and then stay as a steady tint: a button blinking for hours stops being
// read and competes with the scene. More commits arriving pulse again.
class GitAttention {
public:
    static constexpr double PulseSeconds = 6;

    void Update(int behind, double now);
    [[nodiscard]] bool Pulsing(double now) const { return _pulseStart >= 0 && now - _pulseStart < PulseSeconds; }

private:
    int _behind = 0;
    double _pulseStart = -1;
};
