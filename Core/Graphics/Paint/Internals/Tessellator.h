#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "Graphics/Gpu/Internals/PaintVertex.h"
#include "Graphics/Paint/Internals/Triangulator.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Geometry/Affine2.h"
#include "Utilities/Geometry/Float2.h"

class Path;

// Triangles built on the CPU (PaintKind::Solid). Anti-aliasing is a fringe 'aa' wide (one device pixel in the
// output's units) whose outer vertices are transparent; the edge sits in the middle of the fringe.
struct MeshBuffer {
    std::vector<PaintVertex> vertices;
    std::vector<std::uint32_t> indices;
    void Clear() {
        vertices.clear();
        indices.clear();
    }

    // Working memory reused between calls (no allocation once warmed up).
    std::vector<Float2> points;
    std::vector<Float2> directions;
    std::vector<Float2> offsets;
    std::vector<float> lengths;
    std::vector<std::span<const Float2>> rings;
    Triangulator triangulator;
};

struct Contour {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    bool closed = false;
};

// Closed rings in one point list, grouped into regions: a region's first ring is its outline, the others its holes.
struct Regions {
    std::vector<Float2> points;
    std::vector<Contour> rings;
    std::vector<std::uint32_t> ends; // Per region: one past its last ring
    void Clear() {
        points.clear();
        rings.clear();
        ends.clear();
    }
    [[nodiscard]] std::span<const Float2> Ring(std::size_t index) const {
        return std::span<const Float2>(points).subspan(rings[index].first, rings[index].count);
    }
};

struct StrokeParams {
    float width = 1.0f;
    StrokeCap cap = StrokeCap::Butt;
    StrokeJoin join = StrokeJoin::Miter;
    float miterLimit = 4.0f;
    float aa = 1.0f;
};

// Appends a stroked polyline. Lines thinner than 'aa' are drawn 'aa' wide with proportionally less alpha.
void StrokePolyline(MeshBuffer& out, std::span<const Float2> points, bool closed, const StrokeParams& params,
                    PaintColor color);
// Appends a filled polygon (either winding). A self-intersecting outline gets a reasonable fill, not an exact one.
// The anti-aliased edge sits on the outline: triangles come from the outline itself and only their corners move half
// a fringe inwards, so a part thinner than the fringe folds over slightly instead of breaking the triangulation.
void FillPolygon(MeshBuffer& out, std::span<const Float2> points, PaintColor color, float aa);
// Appends every region filled as FillPolygon does, holes left empty.
void FillRegions(MeshBuffer& out, const Regions& regions, PaintColor color, float aa);
// Turns the fill appended since vertex 'first' (drawn in opaque white) into a gradient. Gradient coordinates are
// relative to the shape's centre; toLocal maps target positions into that frame.
void ShadeGradient(MeshBuffer& out, std::size_t first, PaintColor from, PaintColor to, const Gradient& gradient,
                   const Affine2& toLocal);
// Appends 'band' (from ShapeArea::Band) with 'color' fading by distance from 'edge''s rings to transparent at
// 'width': a glow around a shape.
void FadeRegions(MeshBuffer& out, const Regions& band, const Regions& edge, float width, PaintColor color);

// Flattens 'path' after applying 'transform'; curves are split until they deviate less than 'tolerance' (output
// units) from the chords.
void FlattenPath(const Path& path, const Affine2& transform, float tolerance, std::vector<Float2>& points,
                 std::vector<Contour>& contours);
// Points without consecutive duplicates (and without a closing duplicate when 'closed').
void Deduplicate(std::span<const Float2> points, bool closed, std::vector<Float2>& out);
