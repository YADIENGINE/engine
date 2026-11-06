#pragma once
#include <memory>
#include <span>
#include "Graphics/Paint/Internals/Tessellator.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Path.h"
#include "Utilities/Geometry/Float2.h"

// The area enclosed by outlines (Clipper2): resolves fill rules, holes, overlaps and self-intersections, and builds
// the bands along its edge that glows and aligned strokes cover. The arithmetic is exact on a grid of 'unit' steps
// (choose a fraction of a device pixel). The area stays exact between calls: Band works on it, not on a float copy.
class ShapeArea {
public:
    ShapeArea();
    ~ShapeArea();
    ShapeArea(ShapeArea&&) noexcept;
    ShapeArea& operator=(ShapeArea&&) noexcept;

    // Replaces the area with what the closed 'contours' of 'points' enclose under 'rule' (open contours are closed).
    void Set(std::span<const Float2> points, std::span<const Contour> contours, FillRule rule, float unit);
    [[nodiscard]] bool Empty() const;
    // The area as regions: each outline with the holes directly inside it.
    void Get(Regions& out) const;
    // The band between the area's edge and 'distance' away from it: outside when positive, inside when negative.
    // 'join' shapes its corners; a Miter beyond 'miterLimit' times the distance is squared off; Round corners deviate
    // at most 'roundTolerance' from the circle. Parts thinner than the band inside are covered whole.
    void Band(float distance, StrokeJoin join, float miterLimit, float roundTolerance, Regions& out) const;

private:
    struct State;
    std::unique_ptr<State> _state;

public:
    ShapeArea(const ShapeArea&) = delete;
    ShapeArea& operator=(const ShapeArea&) = delete;
};
