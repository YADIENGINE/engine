#pragma once
#include <span>
#include <vector>
#include "Utilities/Float2.h"

// A vector outline built from lines and curves, drawn with Painter::FillPath / StrokePath. Curves are flattened at
// draw time for the pixel size they cover, so one Path stays smooth at any scale. Fill supports simple
// (non-self-intersecting) contours; each contour is filled on its own (no holes).
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

    void Clear();
    [[nodiscard]] bool Empty() const { return _verbs.empty(); }

private:
    friend struct PathAccess;
    enum class Verb { Move, Line, Quad, Cubic, Close };
    void EnsureContour();
    std::vector<Verb> _verbs;
    std::vector<Float2> _points; // Move/Line: 1, Quad: 2, Cubic: 3 per verb
    bool _open = false;
};
