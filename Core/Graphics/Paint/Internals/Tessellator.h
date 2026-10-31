#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include "Graphics/Gpu/Internals/PaintVertex.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Affine2.h"
#include "Utilities/Float2.h"

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
    std::vector<std::uint32_t> remaining;
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
// Appends a filled simple polygon (either winding). Falls back to a fan when ear clipping fails (self-intersection).
void FillPolygon(MeshBuffer& out, std::span<const Float2> points, PaintColor color, float aa);
// Gradient coordinates are relative to the shape's centre; toLocal maps target positions into that frame.
void FillPolygon(MeshBuffer& out, std::span<const Float2> points, PaintColor from, PaintColor to,
                 const Gradient& gradient, const Affine2& toLocal, float aa);
// Appends a band from the polygon's edge outwards 'width' wide, 'color' at the edge fading to transparent.
void PolygonHalo(MeshBuffer& out, std::span<const Float2> points, float width, PaintColor color);

struct Contour {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    bool closed = false;
};
// Flattens 'path' after applying 'transform'; curves are split until they deviate less than 'tolerance' (output
// units) from the chords.
void FlattenPath(const Path& path, const Affine2& transform, float tolerance, std::vector<Float2>& points,
                 std::vector<Contour>& contours);
// Points without consecutive duplicates (and without a closing duplicate when 'closed').
void Deduplicate(std::span<const Float2> points, bool closed, std::vector<Float2>& out);
