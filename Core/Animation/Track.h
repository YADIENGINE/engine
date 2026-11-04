#pragma once
#include <algorithm>
#include <concepts>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include "Animation/AnimationValue.h"
#include "Animation/Curve.h"

// One value at one position. 'curve' shapes the way to this key from the one before, so it reads like Tween::To.
template <class T, class Position> struct TrackKey {
    Position at{};
    T value{};
    Curve curve = Curve::Linear;
};

// One value through several keys, as constant data: it holds no clock, and the caller asks at a position. Before the
// first key it is the first value; after the last, the last. Keys at one position jump there. Whole numbers, flags and
// enums hold until the next key. Invalid keys throw in every configuration.
template <class T, class Position> class BasicTrack {
public:
    using Key = TrackKey<T, Position>;

    BasicTrack(std::initializer_list<Key> keys)
        : BasicTrack(std::vector<Key>(keys)) {}
    explicit BasicTrack(std::vector<Key> keys)
        : _keys(std::move(keys)) {
        if (_keys.empty()) throw std::invalid_argument("A track needs at least one key");
        Position previous{};
        for (const auto& key : _keys) {
            if (!ValidAnimationPosition(key.at)) throw std::invalid_argument("A track key has an invalid position");
            if (key.at < previous) throw std::invalid_argument("A track key goes back in time");
            previous = key.at;
        }
    }

    [[nodiscard]] std::span<const Key> Keys() const { return _keys; }
    [[nodiscard]] Position Length() const { return _keys.back().at; }
    [[nodiscard]] T At(Position position) const { return Sample(static_cast<double>(position)); }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha: from the previous step at
    // alpha 0 to the current one at alpha 1, like StepSmoothed.
    [[nodiscard]] T At(Position position, float alpha) const
        requires std::integral<Position>
    {
        return Sample(static_cast<double>(position) - 1.0 + static_cast<double>(alpha));
    }
    // The value at any position, for code that already works one out.
    [[nodiscard]] T Sample(double position) const {
        const auto next = std::upper_bound(_keys.begin(), _keys.end(), position, [](double at, const Key& key) {
            return at < static_cast<double>(key.at);
        });
        if (next == _keys.begin()) return _keys.front().value;
        if (next == _keys.end()) return _keys.back().value;
        const auto& previous = *(next - 1);
        if constexpr (HeldValue<T>) {
            return previous.value;
        } else {
            const auto from = static_cast<double>(previous.at);
            const auto t = static_cast<float>((position - from) / (static_cast<double>(next->at) - from));
            return MixValue(previous.value, next->value, Ease(next->curve, t));
        }
    }

private:
    std::vector<Key> _keys;
};

// Positions in whole simulation steps: what a simulation uses.
template <class T> using StepTrack = BasicTrack<T, int>;
// Positions in seconds: for a view or a menu on ordinary time.
template <class T> using SecondsTrack = BasicTrack<T, float>;
