#include "Graphics/Paint/Internals/Triangulator.h"
#include <mapbox/earcut.hpp>

namespace mapbox::util {
template <> struct nth<0, Float2> {
    static float get(const Float2& point) { return point.x; }
};
template <> struct nth<1, Float2> {
    static float get(const Float2& point) { return point.y; }
};
} // namespace mapbox::util

// earcut's public function builds a new node pool on every call; its worker object keeps the pool between calls.
struct Triangulator::State {
    mapbox::detail::Earcut<std::uint32_t> earcut;
};

Triangulator::Triangulator()
    : _state(std::make_unique<State>()) {}
Triangulator::~Triangulator() = default;
Triangulator::Triangulator(Triangulator&&) noexcept = default;
Triangulator& Triangulator::operator=(Triangulator&&) noexcept = default;

void Triangulator::Triangulate(std::span<const std::span<const Float2>> rings, std::uint32_t base,
                               std::vector<std::uint32_t>& indices) {
    if (rings.empty()) return;
    auto& earcut = _state->earcut;
    earcut(rings);
    indices.reserve(indices.size() + earcut.indices.size());
    for (const std::uint32_t index : earcut.indices)
        indices.push_back(base + index);
}
