#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "Utilities/Geometry/Float2.h"

// Triangulates polygons with holes (earcut). Either winding works; a self-intersecting outline gets a reasonable fill
// rather than an exact one. Its working memory is kept between calls, so warm calls do not allocate.
class Triangulator {
public:
    Triangulator();
    ~Triangulator();
    Triangulator(Triangulator&&) noexcept;
    Triangulator& operator=(Triangulator&&) noexcept;

    // Appends triangles (three indices each) covering 'rings': the first ring is the outline, the others holes.
    // Points are numbered ring after ring starting at 'base'.
    void Triangulate(std::span<const std::span<const Float2>> rings, std::uint32_t base,
                     std::vector<std::uint32_t>& indices);

private:
    struct State;
    std::unique_ptr<State> _state;

public:
    Triangulator(const Triangulator&) = delete;
    Triangulator& operator=(const Triangulator&) = delete;
};
