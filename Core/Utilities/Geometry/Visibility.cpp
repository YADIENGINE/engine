#include "Utilities/Geometry/Visibility.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Rays towards every corner and every crossing of two edges, and just past each on both sides, against every edge.
// Slower than a sorted sweep, and without its failures on edges that touch, cross or line up with the point, which are
// ordinary in a game's walls.
namespace {
constexpr double Pi = 3.14159265358979323846;
constexpr double BoxMargin = 1.01;    // The outline's box lies past the radius, where a light has already faded out
constexpr double PastCorner = 1e-4;   // Radians beside a corner: at a reach of 1000 units, a tenth of a unit
constexpr double SameAngle = 1e-9;    // Radians
constexpr double EndTolerance = 1e-9; // Share of an edge's length: a ray through its end counts as a hit
constexpr double ThroughPoint = 1e-3; // Units: an edge this close to the point runs through it
constexpr double SameLine = 1e-6;     // Squared units of the area of three points: the middle one adds nothing

// The part of a..b inside the square of half size 'reach' around the origin (Liang-Barsky). False when none is.
bool ClipToBox(double& ax, double& ay, double& bx, double& by, double reach) {
    const double dx = bx - ax;
    const double dy = by - ay;
    double low = 0.0;
    double high = 1.0;
    const auto limit = [&](double direction, double distance) {
        if (direction == 0.0) return distance >= 0.0;
        const double at = distance / direction;
        if (direction < 0.0) {
            if (at > high) return false;
            low = std::max(low, at);
        } else {
            if (at < low) return false;
            high = std::min(high, at);
        }
        return true;
    };
    if (!limit(-dx, ax + reach) || !limit(dx, reach - ax) || !limit(-dy, ay + reach) || !limit(dy, reach - ay))
        return false;
    bx = ax + dx * high;
    by = ay + dy * high;
    ax += dx * low;
    ay += dy * low;
    return true;
}

double SquaredDistanceToOrigin(double ax, double ay, double bx, double by) {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double length = dx * dx + dy * dy;
    const double at = length > 0.0 ? std::clamp(-(ax * dx + ay * dy) / length, 0.0, 1.0) : 0.0;
    const double x = ax + dx * at;
    const double y = ay + dy * at;
    return x * x + y * y;
}
} // namespace

void VisibilityPolygon::AddCorner(double x, double y) {
    const double angle = std::atan2(y, x);
    _angles.push_back(angle - PastCorner);
    _angles.push_back(angle);
    _angles.push_back(angle + PastCorner);
}

void VisibilityPolygon::AddWall(double ax, double ay, double bx, double by) {
    _walls.push_back({ax, ay, bx, by});
    AddCorner(ax, ay);
    AddCorner(bx, by);
}

// Where two walls cross, what is seen turns from one to the other, and neither has a corner there.
void VisibilityPolygon::AddCrossings() {
    for (std::size_t i = 0; i < _walls.size(); ++i) {
        const Wall& one = _walls[i];
        const double oneX = one.bx - one.ax;
        const double oneY = one.by - one.ay;
        for (std::size_t j = i + 1; j < _walls.size(); ++j) {
            const Wall& two = _walls[j];
            const double twoX = two.bx - two.ax;
            const double twoY = two.by - two.ay;
            const double facing = oneX * twoY - oneY * twoX;
            if (facing == 0.0) continue;
            const double betweenX = two.ax - one.ax;
            const double betweenY = two.ay - one.ay;
            const double onOne = (betweenX * twoY - betweenY * twoX) / facing;
            const double onTwo = (betweenX * oneY - betweenY * oneX) / facing;
            if (onOne >= 0.0 && onOne <= 1.0 && onTwo >= 0.0 && onTwo <= 1.0)
                AddCorner(one.ax + oneX * onOne, one.ay + oneY * onOne);
        }
    }
}

std::span<const Float2> VisibilityPolygon::From(Float2 from, float radius, std::span<const VisibilityEdge> edges) {
    _outline.clear();
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(radius) || !(radius > 0.0f)) return _outline;
    _walls.clear();
    _angles.clear();
    const double reach = static_cast<double>(radius) * BoxMargin;
    const double originX = from.x;
    const double originY = from.y;
    for (const VisibilityEdge& edge : edges) {
        double ax = edge.a.x - originX;
        double ay = edge.a.y - originY;
        double bx = edge.b.x - originX;
        double by = edge.b.y - originY;
        if (!std::isfinite(ax) || !std::isfinite(ay) || !std::isfinite(bx) || !std::isfinite(by)) continue;
        if (!ClipToBox(ax, ay, bx, by, reach)) continue;
        if (SquaredDistanceToOrigin(ax, ay, bx, by) < ThroughPoint * ThroughPoint) continue;
        AddWall(ax, ay, bx, by);
    }
    AddCrossings(); // Edges are already cut at the box, so they cross only inside it
    AddWall(-reach, -reach, reach, -reach);
    AddWall(reach, -reach, reach, reach);
    AddWall(reach, reach, -reach, reach);
    AddWall(-reach, reach, -reach, -reach);

    // Angles beside a corner at +-Pi fall outside the range; bring them back so that the order stays one turn.
    for (double& angle : _angles) {
        if (angle > Pi) angle -= 2.0 * Pi;
        if (angle <= -Pi) angle += 2.0 * Pi;
    }
    std::sort(_angles.begin(), _angles.end());
    _angles.erase(std::unique(_angles.begin(), _angles.end(),
                              [](double a, double b) { return b - a < SameAngle; }),
                  _angles.end());

    double lastX = 0.0;
    double lastY = 0.0;
    double beforeX = 0.0;
    double beforeY = 0.0;
    std::size_t kept = 0;
    for (const double angle : _angles) {
        const double directionX = std::cos(angle);
        const double directionY = std::sin(angle);
        double nearest = std::numeric_limits<double>::max();
        for (const Wall& wall : _walls) {
            const double edgeX = wall.bx - wall.ax;
            const double edgeY = wall.by - wall.ay;
            const double facing = directionX * edgeY - directionY * edgeX;
            if (facing == 0.0) continue; // Along the edge: the rays beside the ends find what is there
            const double along = (wall.ax * edgeY - wall.ay * edgeX) / facing;
            if (along < 0.0 || along >= nearest) continue;
            const double on = (wall.ax * directionY - wall.ay * directionX) / facing;
            if (on >= -EndTolerance && on <= 1.0 + EndTolerance) nearest = along;
        }
        if (nearest == std::numeric_limits<double>::max()) continue; // Cannot happen inside the box; a safe outline
        const double x = directionX * nearest;
        const double y = directionY * nearest;
        // A point on the line through the last two adds a triangle and no shape.
        if (kept >= 2 && std::abs((lastX - beforeX) * (y - lastY) - (lastY - beforeY) * (x - lastX)) < SameLine) {
            _outline.back() = {static_cast<float>(originX + x), static_cast<float>(originY + y)};
            lastX = x;
            lastY = y;
            continue;
        }
        _outline.push_back({static_cast<float>(originX + x), static_cast<float>(originY + y)});
        beforeX = lastX;
        beforeY = lastY;
        lastX = x;
        lastY = y;
        ++kept;
    }
    return _outline;
}
