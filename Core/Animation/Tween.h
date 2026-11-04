#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include "Animation/AnimationValue.h"
#include "Animation/Curve.h"

// A value that moves by itself once told where to go. It is the value: keep one as a field, not a pointer to something
// else, so nothing can dangle and a simulation can write it to State. It reads like the plain value.
//
//     SecondsTween<float> scale{1.0f};
//     scale.To(1.4f, 0.3f, Curve::OutBack).Then(1.0f, 0.2f);
//     scale.Update(deltaSeconds);                  // Each frame; UpdateAll covers many
//     paint.Circle(at, 20.0f * scale, Fill(color));
//
// A chain holds up to MaxSegments parts, inline, so a tween copies cheaply and needs no allocation. Longer sequences are
// what a clip is for. Invalid durations and longer chains throw in every configuration.
template <class T, class Position> class BasicTween {
public:
    static constexpr int MaxSegments = 4;

    BasicTween() = default;
    explicit BasicTween(const T& value)
        : _value(value)
        , _previous(value)
        , _from(value) {}

    // Replaces whatever was running and goes from the current value to 'target'.
    BasicTween& To(const T& target, Position duration, Curve curve = Curve::Linear) {
        Stop();
        return Then(target, duration, curve);
    }
    // Then goes on to 'target'. After a finished tween this starts a new chain from the current value.
    BasicTween& Then(const T& target, Position duration, Curve curve = Curve::Linear) {
        Add({target, duration, curve, false});
        return *this;
    }
    // Then holds the value for a while.
    BasicTween& Wait(Position duration) {
        Add({T{}, duration, Curve::Linear, true});
        return *this;
    }
    // Repeats the whole chain, each round from where the last one ended. Such a tween never finishes.
    BasicTween& Loop() {
        Position total{};
        for (int i = 0; i < _count; ++i)
            total += _segments[static_cast<std::size_t>(i)].duration;
        if (!(total > Position{})) throw std::invalid_argument("A looping tween needs a duration");
        _loop = true;
        return *this;
    }
    // Stays where it is, and forgets the rest of the chain.
    void Stop() {
        _from = _value;
        _count = 0;
        _index = 0;
        _clock = Position{};
        _loop = false;
    }
    // Jumps to 'value' with no movement in between, and forgets the chain.
    void Snap(const T& value) {
        _value = value;
        _previous = value;
        Stop();
    }

    // Advances; true when the chain ended in this call. Call once per frame or step, even when nothing is running.
    bool Update(Position amount) {
        _previous = _value;
        _justFinished = false;
        if (!(amount > Position{}) || _index >= _count) return false;
        _clock += amount;
        while (_index < _count && _clock >= Current().duration) {
            _clock -= Current().duration;
            if (!Current().hold) _from = Current().target;
            ++_index;
            if (_index == _count && _loop) _index = 0;
        }
        if (_index >= _count) {
            _clock = Position{};
            _value = _from;
            _justFinished = true;
            return true;
        }
        const auto& segment = Current();
        _value = segment.hold ? _from : MixValue(_from, segment.target, Progress(segment));
        return false;
    }
    bool Step()
        requires std::integral<Position>
    {
        return Update(1);
    }

    [[nodiscard]] const T& Value() const { return _value; }
    [[nodiscard]] operator const T&() const { return _value; }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha.
    [[nodiscard]] T Value(float alpha) const
        requires std::integral<Position>
    {
        return alpha >= 1.0f ? _value : MixValue(_previous, _value, alpha);
    }
    // A chain is running.
    [[nodiscard]] bool Active() const { return _index < _count; }
    // The chain ended in the last Update.
    [[nodiscard]] bool JustFinished() const { return _justFinished; }

    // Everything that decides later steps, for a simulation's State.
    template <class Visitor> void State(Visitor& state) const {
        state.Field("value", _value);
        state.Field("previous", _previous);
        state.Field("from", _from);
        state.Field("clock", _clock);
        state.Field("index", _index);
        state.Field("loop", _loop);
        state.Field("justFinished", _justFinished);
        for (int i = 0; i < _count; ++i) {
            const auto& segment = _segments[static_cast<std::size_t>(i)];
            state.Group("segment", static_cast<std::size_t>(i), [&] {
                state.Field("target", segment.target);
                state.Field("duration", segment.duration);
                state.Field("curve", segment.curve);
                state.Field("hold", segment.hold);
            });
        }
    }

private:
    struct Segment {
        T target{};
        Position duration{};
        Curve curve = Curve::Linear;
        bool hold = false;
    };

    [[nodiscard]] const Segment& Current() const { return _segments[static_cast<std::size_t>(_index)]; }
    [[nodiscard]] float Progress(const Segment& segment) const {
        return Ease(segment.curve, static_cast<float>(static_cast<double>(_clock) / static_cast<double>(segment.duration)));
    }
    void Add(const Segment& segment) {
        if (!ValidAnimationPosition(segment.duration)) throw std::invalid_argument("A tween has an invalid duration");
        if (_loop) throw std::invalid_argument("A looping tween cannot be extended; start again with To");
        if (_index >= _count) Stop(); // A finished chain: start a new one from here
        if (_count >= MaxSegments) throw std::invalid_argument("A tween chain is full; use a clip for longer sequences");
        _segments[static_cast<std::size_t>(_count)] = segment;
        ++_count;
    }

    std::array<Segment, MaxSegments> _segments{};
    T _value{};
    T _previous{};
    T _from{};
    Position _clock{};
    int _count = 0;
    int _index = 0;
    bool _loop = false;
    bool _justFinished = false;
};

// Positions in whole simulation steps: what a simulation uses.
template <class T> using StepTween = BasicTween<T, int>;
// Positions in seconds: for a view or a menu on ordinary time.
template <class T> using SecondsTween = BasicTween<T, float>;

template <class T> inline constexpr bool IsAnimationTween = false;
template <class T, class Position> inline constexpr bool IsAnimationTween<BasicTween<T, Position>> = true;

// Advances several tweens in one line: UpdateAll(deltaSeconds, scale, alpha, offset). In a simulation pass 1.
template <class Amount, class... Tweens>
    requires(IsAnimationTween<Tweens> && ...)
void UpdateAll(Amount amount, Tweens&... tweens) {
    (tweens.Update(amount), ...);
}
// Advances tweens that are fields of each item of a list: UpdateAll(deltaSeconds, enemies, &Enemy::scale, &Enemy::alpha).
// It stores nothing, so the list may grow and shrink freely.
template <class Amount, std::ranges::range Items, class... Members>
    requires(sizeof...(Members) > 0 && (std::is_member_object_pointer_v<Members> && ...))
void UpdateAll(Amount amount, Items&& items, Members... members) {
    for (auto& item : items)
        ((item.*members).Update(amount), ...);
}
