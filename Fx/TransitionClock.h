#pragma once
#include <optional>

class TransitionClock {
public:
    TransitionClock(float closeSeconds, float holdSeconds, float openSeconds);
    void Start();
    [[nodiscard]] bool Update(float deltaSeconds);
    [[nodiscard]] bool Active() const { return _elapsed.has_value(); }
    [[nodiscard]] float Cover(bool& opening) const;

private:
    float _closeSeconds;
    float _holdSeconds;
    float _openSeconds;
    std::optional<float> _elapsed;
};
