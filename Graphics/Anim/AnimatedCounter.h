#pragma once

struct AnimatedCounterSettings {
    float speed = 0.0f;
    float punchSeconds = 0.0f;
    double snapDistance = 0.5;
};

class AnimatedCounter {
public:
    explicit AnimatedCounter(const AnimatedCounterSettings& settings = {});
    void Update(int target, float deltaSeconds);
    void Clear();
    [[nodiscard]] double Shown() const { return _shown; }
    [[nodiscard]] int Target() const { return _target; }
    [[nodiscard]] float Punch() const { return _punch; }

private:
    AnimatedCounterSettings _settings;
    double _shown = 0.0;
    int _target = 0;
    float _punch = 0.0f;
};
