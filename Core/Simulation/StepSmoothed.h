#pragma once

// A value a simulation changes once per step and a view draws between steps. Keep it in the simulation, hash Current()
// only, and call At(runner.Alpha()) when drawing. Set moves it; Snap teleports it, so no line is drawn across the jump.
// T needs +, - and * float (float, Float2).
template <class T> class StepSmoothed {
public:
    constexpr StepSmoothed() = default;
    explicit constexpr StepSmoothed(T value)
        : _previous(value)
        , _current(value) {}
    constexpr void Set(T value) {
        _previous = _current;
        _current = value;
    }
    constexpr void Snap(T value) {
        _previous = value;
        _current = value;
    }
    [[nodiscard]] constexpr const T& Current() const { return _current; }
    [[nodiscard]] constexpr T At(float alpha) const { return _previous + (_current - _previous) * alpha; }

private:
    T _previous{};
    T _current{};
};
