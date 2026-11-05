#pragma once
#include <cstddef>
#include <span>
#include <vector>
#include "Utilities/Geometry/Affine2.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/Visibility.h"

// What stops light: outlines and lines, in the coordinates of the lights. Independent of collision, because what stops
// light and what stops a player differ: glass, a hitbox smaller than its sprite, a background pillar. Walls that do
// match their colliders come from AddOccluders (CollisionOccluders.h). Keep a set for what stands still and fill
// another for what moves, from the positions the view draws. Invalid points throw std::invalid_argument.
class LightOccluders {
public:
    // A closed shape, in either direction; it need not be convex. With 'selfLit' its near side and inside are lit and
    // the shadow starts behind it, which suits a character or a wall seen from above. Without it, light stops at its
    // near side. Either way it does not stop a light inside it: a torch in a wall still lights the room. 3 points or
    // more.
    void Outline(std::span<const Float2> closed, const Affine2& at = {}, bool selfLit = true);
    // A wall without thickness; it stops light from both sides. 2 points or more.
    void Line(std::span<const Float2> open, const Affine2& at = {});
    void Clear();
    [[nodiscard]] bool Empty() const { return _edges.empty(); }
    // Every edge, for drawing them.
    [[nodiscard]] std::span<const VisibilityEdge> Edges() const { return _edges; }
    // Appends the edges that stop a light at 'from' reaching 'radius'.
    void AppendBlocking(Float2 from, float radius, std::vector<VisibilityEdge>& edges) const;

private:
    struct Part {
        std::size_t first = 0;
        std::size_t count = 0;
        Float2 low;
        Float2 high;
        bool closed = false;
        bool selfLit = false;
    };
    void AddPart(std::span<const Float2> points, const Affine2& at, bool closed, bool selfLit);

    std::vector<VisibilityEdge> _edges;
    std::vector<Part> _parts;
    std::vector<Float2> _placed; // Working memory
};
