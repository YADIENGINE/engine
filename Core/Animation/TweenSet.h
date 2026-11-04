#pragma once
#include <concepts>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>
#include "Animation/Tween.h"

// Tweens that need no field of their own, found by a key: an enum, or an object's id. It owns the values, so no pointer
// can dangle, and one Update covers them all. Keys are visited in order, so a run replays the same.
//
//     SecondsTweenSet<MenuPart> tweens{1.0f};               // Value of a key not seen yet
//     tweens.To(MenuPart::Title, 1.4f, 0.3f, Curve::OutBack).Then(1.0f, 0.2f);
//     tweens.Update(deltaSeconds, [&](MenuPart finished) { ... });
//     paint.Circle(at, 20.0f * tweens[MenuPart::Title], Fill(color));
//
// A key stays with its last value until Remove or Clear, so remove the ids of objects that are gone.
template <class Key, class T, class Position> class BasicTweenSet {
public:
    using Tween = BasicTween<T, Position>;

    BasicTweenSet() = default;
    explicit BasicTweenSet(const T& fallback)
        : _fallback(fallback) {}

    // Replaces what the key was doing. The reference is for chaining Then, in the same statement.
    Tween& To(const Key& key, const T& target, Position duration, Curve curve = Curve::Linear) {
        Tween& tween = Find(key);
        tween.To(target, duration, curve);
        return tween;
    }
    // Jumps to 'value' with no movement in between, and forgets the chain.
    void Set(const Key& key, const T& value) { Find(key).Snap(value); }
    void Remove(const Key& key) { _tweens.erase(key); }
    void Clear() { _tweens.clear(); }

    // Advances all of them. With a callback, calls it with each key whose chain ended, in key order, after all have
    // moved; it may change the set.
    void Update(Position amount) {
        for (auto& [key, tween] : _tweens)
            tween.Update(amount);
    }
    template <class Fn> void Update(Position amount, Fn&& onFinished) {
        std::vector<Key> finished;
        for (auto& [key, tween] : _tweens)
            if (tween.Update(amount)) finished.push_back(key);
        for (const Key& key : finished)
            onFinished(key);
    }
    void Step()
        requires std::integral<Position>
    {
        Update(1);
    }
    template <class Fn>
    void Step(Fn&& onFinished)
        requires std::integral<Position>
    {
        Update(1, onFinished);
    }

    [[nodiscard]] T Value(const Key& key) const {
        const auto found = _tweens.find(key);
        return found == _tweens.end() ? _fallback : found->second.Value();
    }
    [[nodiscard]] T operator[](const Key& key) const { return Value(key); }
    // The same for a view drawing between steps, with 'alpha' from SimulationRunner::Alpha.
    [[nodiscard]] T Value(const Key& key, float alpha) const
        requires std::integral<Position>
    {
        const auto found = _tweens.find(key);
        return found == _tweens.end() ? _fallback : found->second.Value(alpha);
    }
    [[nodiscard]] bool Contains(const Key& key) const { return _tweens.contains(key); }
    [[nodiscard]] bool Active(const Key& key) const {
        const auto found = _tweens.find(key);
        return found != _tweens.end() && found->second.Active();
    }
    [[nodiscard]] std::size_t Count() const { return _tweens.size(); }

    // Everything that decides later steps, for a simulation's State.
    template <class Visitor> void State(Visitor& state) const {
        state.Field("count", _tweens.size());
        std::size_t index = 0;
        for (const auto& [key, tween] : _tweens) {
            state.Group("tween", index, [&] {
                state.Field("key", key);
                tween.State(state);
            });
            ++index;
        }
    }

private:
    [[nodiscard]] Tween& Find(const Key& key) { return _tweens.try_emplace(key, _fallback).first->second; }

    std::map<Key, Tween> _tweens;
    T _fallback{};
};

// Positions in whole simulation steps: what a simulation uses.
template <class Key, class T = float> using StepTweenSet = BasicTweenSet<Key, T, int>;
// Positions in seconds: for a view or a menu on ordinary time.
template <class Key, class T = float> using SecondsTweenSet = BasicTweenSet<Key, T, float>;
