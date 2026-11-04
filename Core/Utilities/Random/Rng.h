#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>

// The engine's seeded random generator (PCG32): the same seed and stream give the same sequence in every build, because
// no standard library distribution is involved. There is no global generator: each owner keeps its own, so visual
// randomness cannot move a simulation's sequence. Different streams with one seed are independent sequences.
class Rng {
public:
    explicit constexpr Rng(std::uint64_t seed = 0, std::uint64_t stream = 0)
        : _increment((stream << 1U) | 1U) {
        (void)Next();
        _state += seed;
        (void)Next();
    }
    [[nodiscard]] constexpr std::uint32_t Next() {
        const std::uint64_t old = _state;
        _state = old * 6364136223846793005ULL + _increment;
        const auto mixed = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
        const auto rotation = static_cast<std::uint32_t>(old >> 59U);
        return (mixed >> rotation) | (mixed << ((32U - rotation) & 31U));
    }
    // A uniform value in 0..count-1 without modulo bias; count > 0.
    [[nodiscard]] constexpr std::uint32_t Below(std::uint32_t count) {
        const std::uint32_t threshold = (0U - count) % count;
        for (;;) {
            const std::uint32_t value = Next();
            if (value >= threshold) return value % count;
        }
    }
    // A uniform whole number in low..high, both included; low <= high.
    [[nodiscard]] constexpr int Int(int low, int high) {
        const auto span = static_cast<std::uint32_t>(static_cast<std::int64_t>(high) - low + 1);
        if (span == 0U) return static_cast<int>(Next()); // The whole int range
        return static_cast<int>(static_cast<std::int64_t>(low) + Below(span));
    }
    // A uniform index in 0..count-1; count > 0.
    [[nodiscard]] constexpr std::size_t Index(std::size_t count) {
        return static_cast<std::size_t>(Below(static_cast<std::uint32_t>(count)));
    }
    // Uniform in [0, 1).
    [[nodiscard]] constexpr float Float01() { return static_cast<float>(Next() >> 8U) * (1.0f / 16777216.0f); }
    [[nodiscard]] constexpr float Float(float low, float high) { return low + (high - low) * Float01(); }
    // Uniform in [0, 1), with more resolution than Float01.
    [[nodiscard]] constexpr double Double01() {
        const std::uint64_t high = Next() >> 5U;
        const std::uint64_t low = Next() >> 6U;
        return static_cast<double>((high << 26U) | low) * (1.0 / 9007199254740992.0);
    }
    // True with the given probability.
    [[nodiscard]] constexpr bool Chance(float probability) { return Float01() < probability; }
    // Fisher-Yates over any random access container.
    template <class Container> constexpr void Shuffle(Container& items) {
        for (std::size_t i = items.size(); i > 1; --i)
            std::swap(items[i - 1], items[Index(i)]);
    }
    // The whole generator state, for state hashing and dumps.
    [[nodiscard]] constexpr std::uint64_t State() const { return _state; }
    [[nodiscard]] constexpr std::uint64_t Increment() const { return _increment; }
    friend constexpr bool operator==(const Rng&, const Rng&) = default;

private:
    std::uint64_t _state = 0;
    std::uint64_t _increment = 1;
};
