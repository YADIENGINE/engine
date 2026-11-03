#pragma once
#include <cstdint>
#include <type_traits>

// A set of collision layers. A game declares its layers as an enum (at most 64) and passes them through MaskOf.
struct CollisionMask {
    std::uint64_t bits = 0;

    [[nodiscard]] static constexpr CollisionMask All() { return {~std::uint64_t{0}}; }
    [[nodiscard]] constexpr bool Overlaps(CollisionMask other) const { return (bits & other.bits) != 0U; }
    friend constexpr CollisionMask operator|(CollisionMask a, CollisionMask b) { return {a.bits | b.bits}; }
    friend constexpr bool operator==(CollisionMask, CollisionMask) = default;
};

// MaskOf(Layer::Enemy, Layer::Wall)
template <class... Layers>
    requires(sizeof...(Layers) > 0 && (std::is_enum_v<Layers> && ...))
[[nodiscard]] constexpr CollisionMask MaskOf(Layers... layers) {
    return {((std::uint64_t{1} << static_cast<std::uint64_t>(layers)) | ...)};
}
