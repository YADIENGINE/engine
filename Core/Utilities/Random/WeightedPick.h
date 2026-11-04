#pragma once
#include <cstddef>
#include "Utilities/Random/Rng.h"

// Weighted random choice: returns an index in proportion to 'weights' (zero and negative weights never occur; at least
// one must be > 0). The same generator state gives the same index in every build. Header-only.
template <class Weights> [[nodiscard]] std::size_t WeightedIndex(const Weights& weights, Rng& rng) {
    double total = 0.0;
    for (const auto& weight : weights)
        if (weight > 0) total += static_cast<double>(weight);
    double remaining = rng.Double01() * total;
    std::size_t index = 0, last = 0;
    for (const auto& weight : weights) {
        if (weight > 0) {
            remaining -= static_cast<double>(weight);
            if (remaining < 0.0) return index;
            last = index; // Rounding can leave a tiny remainder after the last positive weight
        }
        ++index;
    }
    return last;
}
