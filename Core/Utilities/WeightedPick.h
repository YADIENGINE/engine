#pragma once
#include <cstddef>
#include <iterator>
#include <random>

// Ağırlıklı rastgele seçim: 'weights' oranında bir sıra döndürür (ağırlık 0 olan hiç çıkmaz; en az biri > 0 olmalı).
// std::discrete_distribution'ı her çağrıda aynı biçimde kurar, böylece aynı seed aynı diziyi verir. Header-only.
template <class Weights, class Rng> [[nodiscard]] std::size_t WeightedIndex(const Weights& weights, Rng& rng) {
    std::discrete_distribution<std::size_t> pick(std::begin(weights), std::end(weights));
    return pick(rng);
}
