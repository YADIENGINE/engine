#pragma once
#include <cstddef>
#include <iterator>
#include <random>

// Weighted random choice: returns an index in proportion to 'weights' (zero weights never occur; at least one must
// be > 0). Builds std::discrete_distribution identically on each call so the same seed gives the same sequence.
// Header-only.
template <class Weights, class Rng> [[nodiscard]] std::size_t WeightedIndex(const Weights& weights, Rng& rng) {
    std::discrete_distribution<std::size_t> pick(std::begin(weights), std::end(weights));
    return pick(rng);
}
