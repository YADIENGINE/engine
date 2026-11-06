#pragma once
#include <span>
#include <vector>
#include "Utilities/Geometry/Float2.h"

// Which parts of a plane a path's contours enclose, for filling. NonZero (default, as SVG and Canvas): a point is
// inside when the contours around it turn more times one way than the other, so overlapping contours merge and a hole
// is a contour running the other way (points in reverse order, or ArcTo with a negative sweep). EvenOdd: a point is
// inside when an odd number of contours surround it, so any contour inside another cuts a hole whatever its direction.
enum class FillRule { NonZero, EvenOdd };

// A vector outline built from lines and curves, drawn with Painter::FillPath / StrokePath. Curves are flattened at
// draw time for the pixel size they cover, so one Path stays smooth at any scale. Filling closes every contour and
// fills the area they enclose under the path's FillRule: holes, overlapping and self-intersecting contours all work.
//     Path ring;
//     ring.SetFillRule(FillRule::EvenOdd).AddCircle(center, 40.0f).AddCircle(center, 25.0f);
//     paint.FillPath(ring, Fill(gold)); // A ring with a hole
class Path {
public:
    // Starts a new contour.
    Path& MoveTo(Float2 point);
    // The following segments continue the current contour (MoveTo is implied at the origin if none started).
    Path& LineTo(Float2 point);
    Path& QuadTo(Float2 control, Float2 to);
    Path& CubicTo(Float2 control1, Float2 control2, Float2 to);
    // Circular arc around 'center' from 'startAngle' over 'sweep' radians (clockwise when positive); connects to the
    // current point with a line.
    Path& ArcTo(Float2 center, float radius, float startAngle, float sweep);
    // Joins the contour's end to its start.
    Path& Close();

    // Closed contours of common shapes.
    Path& AddPolygon(std::span<const Float2> points);
    // Closed regular polygon: 'sides' corners on a circle of 'radius', the first 'radians' clockwise from +x.
    Path& AddRegularPolygon(Float2 center, int sides, float radius, float radians);
    Path& AddCircle(Float2 center, float radius);

    // How filling decides what is inside (FillRule). Kept by Clear.
    Path& SetFillRule(FillRule rule);
    [[nodiscard]] FillRule Rule() const { return _rule; }

    // Removes every contour.
    void Clear();
    [[nodiscard]] bool Empty() const { return _verbs.empty(); }

private:
    friend struct PathAccess;
    enum class Verb { Move, Line, Quad, Cubic, Close };
    void EnsureContour();
    std::vector<Verb> _verbs;
    std::vector<Float2> _points; // Move/Line: 1, Quad: 2, Cubic: 3 per verb
    bool _open = false;
    FillRule _rule = FillRule::NonZero;
};
