#pragma once
#include <span>
#include <vector>
#include "Utilities/Geometry/Float2.h"

// A straight piece of something that blocks sight.
struct VisibilityEdge {
    Float2 a;
    Float2 b;
};

// What can be seen from a point past straight obstacles, for drawing: a light's reach, a guard's view. Keep one and
// reuse it, since it holds its working memory. For views: a simulation does not link it, and asks
// CollisionWorld::Raycast.
class VisibilityPolygon {
public:
    // The outline of what 'from' sees within 'radius', in order around 'from', so that a fan from 'from' fills it. It
    // reaches a little past 'radius' where nothing blocks the way. An edge blocks up to and including its ends: nothing
    // passes between two that share a corner. An edge running through 'from' itself blocks nothing. Empty for an
    // invalid point or radius. Valid until the next call.
    [[nodiscard]] std::span<const Float2> From(Float2 from, float radius, std::span<const VisibilityEdge> edges);

private:
    struct Wall {
        double ax;
        double ay;
        double bx;
        double by;
    };
    void AddCorner(double x, double y);
    void AddWall(double ax, double ay, double bx, double by);
    void AddCrossings();

    std::vector<Wall> _walls;
    std::vector<double> _angles;
    std::vector<Float2> _outline;
};
