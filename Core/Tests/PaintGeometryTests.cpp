#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>
#include "Graphics/Paint/Internals/ShapeArea.h"
#include "Graphics/Paint/Internals/Tessellator.h"
#include "Graphics/Paint/Path.h"
#include "Utilities/Geometry/Affine2.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"
#include "Utilities/Math/Scalar.h"

using Catch::Approx;

namespace {
constexpr PaintColor Opaque{255, 255, 255, 255};

float CoveredArea(const MeshBuffer& mesh) {
    // Area of the fully opaque triangles (the core), ignoring the fringe.
    float area = 0.0f;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const PaintVertex& a = mesh.vertices[mesh.indices[i]];
        const PaintVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const PaintVertex& c = mesh.vertices[mesh.indices[i + 2]];
        if (a.color.a != 255 || b.color.a != 255 || c.color.a != 255) continue;
        area += std::abs(Cross(Float2{b.x - a.x, b.y - a.y}, Float2{c.x - a.x, c.y - a.y})) * 0.5f;
    }
    return area;
}

bool Near(Float2 a, Float2 b, float tolerance = 1e-4f) { return Length(a - b) <= tolerance; }

constexpr float Unit = 1.0f / 256.0f; // ShapeArea grid of the tests (aa = 1)

std::vector<Float2> Square(Float2 at, float size, bool clockwise = true) {
    std::vector<Float2> square{at, at + Float2{size, 0.0f}, at + Float2{size, size}, at + Float2{0.0f, size}};
    if (!clockwise) std::reverse(square.begin(), square.end());
    return square;
}

// The area 'outlines' enclose under 'rule', filled with a negligible fringe; returns its opaque area.
float FilledArea(const std::vector<std::vector<Float2>>& outlines, FillRule rule) {
    std::vector<Float2> points;
    std::vector<Contour> contours;
    for (const auto& outline : outlines) {
        contours.push_back(
            {static_cast<std::uint32_t>(points.size()), static_cast<std::uint32_t>(outline.size()), true});
        points.insert(points.end(), outline.begin(), outline.end());
    }
    ShapeArea area;
    area.Set(points, contours, rule, Unit);
    Regions regions;
    area.Get(regions);
    MeshBuffer mesh;
    FillRegions(mesh, regions, Opaque, 0.001f);
    return CoveredArea(mesh);
}

float DistanceToPolygon(Float2 point, std::span<const Float2> polygon) {
    float nearest = 1e30f;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Float2 a = polygon[i];
        const Float2 edge = polygon[(i + 1) % polygon.size()] - a;
        const float t = std::clamp(Dot(point - a, edge) / std::max(LengthSquared(edge), 1e-12f), 0.0f, 1.0f);
        nearest = std::min(nearest, Length(point - (a + edge * t)));
    }
    return nearest;
}

bool InsidePolygon(Float2 point, std::span<const Float2> polygon) {
    bool inside = false;
    for (std::size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Float2 a = polygon[i];
        const Float2 b = polygon[j];
        if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
            inside = !inside;
    }
    return inside;
}

float TriangleArea(const MeshBuffer& mesh) {
    float covered = 0.0f;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const PaintVertex& a = mesh.vertices[mesh.indices[i]];
        const PaintVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const PaintVertex& c = mesh.vertices[mesh.indices[i + 2]];
        covered += std::abs(Cross(Float2{b.x - a.x, b.y - a.y}, Float2{c.x - a.x, c.y - a.y})) * 0.5f;
    }
    return covered;
}

std::vector<Float2> Centroids(const MeshBuffer& mesh) {
    std::vector<Float2> centroids;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const PaintVertex& a = mesh.vertices[mesh.indices[i]];
        const PaintVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const PaintVertex& c = mesh.vertices[mesh.indices[i + 2]];
        centroids.push_back({(a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f});
    }
    return centroids;
}
} // namespace

TEST_CASE("Engine.PaintMath.AffineComposesInvertsAndRotatesAroundPivots", "[engine][paint]") {
    const Affine2 move = Affine2::Translate({10.0f, 0.0f});
    const Affine2 scale = Affine2::Scale(2.0f);
    CHECK(Near((move * scale).Apply({1.0f, 1.0f}), {12.0f, 2.0f})); // Inner (scale) first
    const Affine2 turn = Affine2::Rotate(Pi / 2.0f, {5.0f, 5.0f});
    CHECK(Near(turn.Apply({5.0f, 5.0f}), {5.0f, 5.0f}));
    CHECK(Near(turn.Apply({6.0f, 5.0f}), {5.0f, 6.0f})); // Clockwise on screen (y down)
    const Affine2 any = Affine2::Rotate(0.7f, {3.0f, -2.0f}) * Affine2::Scale({2.0f, 0.5f}) * move;
    CHECK(Near((any.Inverse() * any).Apply({4.0f, 9.0f}), {4.0f, 9.0f}));
    const Affine2 camera = Affine2::Camera({100.0f, 50.0f}, {400.0f, 300.0f}, 2.0f, 0.3f);
    CHECK(Near(camera.Apply({100.0f, 50.0f}), {400.0f, 300.0f}, 1e-3f));
    CHECK(camera.UniformScale() == Approx(2.0f));
    CHECK(Affine2::Scale(3.0f).AxisAligned());
    CHECK_FALSE(turn.AxisAligned());
}

TEST_CASE("Engine.PaintMath.RectsIntersectUnionAndInset", "[engine][paint]") {
    const FloatRect a{0.0f, 0.0f, 10.0f, 10.0f};
    const FloatRect b{5.0f, 6.0f, 10.0f, 10.0f};
    CHECK(a.Intersect(b) == FloatRect{5.0f, 6.0f, 5.0f, 4.0f});
    CHECK(a.Intersect({20.0f, 20.0f, 1.0f, 1.0f}).Empty());
    CHECK(a.Union(b) == FloatRect{0.0f, 0.0f, 15.0f, 16.0f});
    CHECK(a.Union(FloatRect{}) == a);
    CHECK(a.Inset(2.0f) == FloatRect{2.0f, 2.0f, 6.0f, 6.0f});
    CHECK(a.Inset(8.0f).Empty());
    CHECK(FloatRect::FromCorners({4.0f, 9.0f}, {1.0f, 2.0f}) == FloatRect{1.0f, 2.0f, 3.0f, 7.0f});
    CHECK(a.Contains({0.0f, 0.0f}));
    CHECK_FALSE(a.Contains({10.0f, 5.0f}));
}

TEST_CASE("Engine.PaintMath.ColorHelpers", "[engine][paint]") {
    CHECK(Rgb(0x102030) == Color4{0x10, 0x20, 0x30, 255});
    CHECK(WithAlpha(Rgb(0xFFFFFF, 10), 0.5f).a == 128);
    CHECK(ScaleAlpha(Rgb(0xFFFFFF, 100), 0.5f).a == 50);
    CHECK(Mix(Color4{0, 0, 0, 0}, Color4{200, 100, 50, 255}, 0.5f) == Color4{100, 50, 25, 128});
    CHECK(Mix(Color4{}, Color4{200, 0, 0, 255}, 3.0f).r == 200);
    CHECK(Hsv(0.0f, 1.0f, 1.0f) == Color4{255, 0, 0, 255});
    CHECK(Hsv(120.0f, 1.0f, 1.0f) == Color4{0, 255, 0, 255});
    CHECK(Hsv(240.0f, 1.0f, 1.0f, 7) == Color4{0, 0, 255, 7});
    CHECK(Hsv(-120.0f, 1.0f, 1.0f) == Hsv(240.0f, 1.0f, 1.0f));  // Hue wraps
    CHECK(Hsv(60.0f, 0.0f, 0.5f) == Color4{128, 128, 128, 255}); // No saturation: grey
}

TEST_CASE("Engine.Tessellator.StrokeCoversItsWidth", "[engine][paint]") {
    MeshBuffer mesh;
    const std::vector<Float2> line{{0.0f, 0.0f}, {100.0f, 0.0f}};
    StrokePolyline(mesh, line, false, {.width = 10.0f, .aa = 1.0f}, Opaque);
    // Butt caps: the opaque core is (length - aa) x (width - aa).
    CHECK(CoveredArea(mesh) == Approx(99.0f * 9.0f).epsilon(0.01));
    for (const PaintVertex& vertex : mesh.vertices) {
        CHECK(std::abs(vertex.y) <= 5.5f + 1e-4f);
        CHECK(vertex.x >= -0.5f - 1e-4f);
        CHECK(vertex.x <= 100.5f + 1e-4f);
    }
}

TEST_CASE("Engine.Tessellator.HairlinesFadeInsteadOfThinning", "[engine][paint]") {
    MeshBuffer mesh;
    const std::vector<Float2> line{{0.0f, 0.0f}, {10.0f, 0.0f}};
    StrokePolyline(mesh, line, false, {.width = 0.25f, .aa = 1.0f}, Opaque);
    std::uint8_t strongest = 0;
    for (const PaintVertex& vertex : mesh.vertices)
        strongest = std::max(strongest, vertex.color.a);
    CHECK(strongest == 64);
}

TEST_CASE("Engine.Tessellator.JoinsStayWithinTheirLimits", "[engine][paint]") {
    const std::vector<Float2> sharp{{0.0f, 0.0f}, {50.0f, 2.0f}, {0.0f, 4.0f}};
    for (const StrokeJoin join : {StrokeJoin::Miter, StrokeJoin::Bevel, StrokeJoin::Round}) {
        MeshBuffer mesh;
        StrokePolyline(mesh, sharp, false, {.width = 4.0f, .join = join, .miterLimit = 4.0f, .aa = 1.0f}, Opaque);
        REQUIRE_FALSE(mesh.indices.empty());
        for (const PaintVertex& vertex : mesh.vertices)
            CHECK(vertex.x <= 50.0f + 2.0f * 4.0f + 1.0f); // A miter past the limit falls back to a bevel
    }
    MeshBuffer closed;
    const std::vector<Float2> square{{0.0f, 0.0f}, {10.0f, 0.0f}, {10.0f, 10.0f}, {0.0f, 10.0f}};
    StrokePolyline(closed, square, true, {.width = 2.0f, .aa = 1.0f}, Opaque);
    // Ring 12x12 outer, 8x8 inner, shrunk by the fringe on both sides.
    CHECK(CoveredArea(closed) == Approx(11.0f * 11.0f - 9.0f * 9.0f).epsilon(0.02));
}

TEST_CASE("Engine.Tessellator.FillsConvexAndConcavePolygonsOfEitherWinding", "[engine][paint]") {
    const std::vector<Float2> notch{{0.0f, 0.0f}, {30.0f, 0.0f}, {30.0f, 30.0f}, {15.0f, 10.0f}, {0.0f, 30.0f}};
    std::vector<Float2> reversed(notch.rbegin(), notch.rend());
    const float exact = 30.0f * 30.0f - 0.5f * 30.0f * 20.0f;
    for (const auto& polygon : {notch, reversed}) {
        MeshBuffer mesh;
        FillPolygon(mesh, polygon, Opaque, 0.01f);
        CHECK(CoveredArea(mesh) == Approx(exact).epsilon(0.01));
    }
}

TEST_CASE("Engine.Tessellator.PathsFlattenWithinTolerance", "[engine][paint]") {
    Path circle;
    circle.AddCircle({0.0f, 0.0f}, 100.0f);
    std::vector<Float2> points;
    std::vector<Contour> contours;
    FlattenPath(circle, Affine2::Identity(), 0.25f, points, contours);
    REQUIRE(contours.size() == 1);
    CHECK(contours[0].closed);
    const std::size_t fine = points.size();
    for (const Float2 point : points)
        CHECK(Length(point) == Approx(100.0f).margin(0.3f));
    FlattenPath(circle, Affine2::Scale(0.1f), 0.25f, points, contours);
    CHECK(points.size() < fine); // Smaller on screen: fewer segments
    Path open;
    open.MoveTo({0.0f, 0.0f})
        .LineTo({10.0f, 0.0f})
        .MoveTo({0.0f, 5.0f})
        .CubicTo({3.0f, 9.0f}, {6.0f, 9.0f}, {9.0f, 5.0f});
    FlattenPath(open, Affine2::Identity(), 0.1f, points, contours);
    REQUIRE(contours.size() == 2);
    CHECK_FALSE(contours[1].closed);
    CHECK(Near(points[contours[1].first + contours[1].count - 1], {9.0f, 5.0f}));
}

TEST_CASE("Engine.Path.RegularPolygonsAppendClosedClockwiseContours", "[engine][paint]") {
    Path path;
    path.AddRegularPolygon({10.0f, 20.0f}, 4, 5.0f, Pi * 0.5f);
    std::vector<Float2> points;
    std::vector<Contour> contours;
    FlattenPath(path, {}, 0.1f, points, contours);
    REQUIRE(contours.size() == 1);
    CHECK(contours[0].closed);
    REQUIRE(points.size() == 4);
    CHECK(Near(points[0], {10.0f, 25.0f}));
    CHECK(Near(points[1], {5.0f, 20.0f}));
    CHECK(Near(points[2], {10.0f, 15.0f}));
    CHECK(Near(points[3], {15.0f, 20.0f}));
    const auto before = points;
    for (int sides : {-1, 0, 1, 2})
        path.AddRegularPolygon({}, sides, 10.0f, 0.0f);
    FlattenPath(path, {}, 0.1f, points, contours);
    CHECK(points == before);
    REQUIRE(contours.size() == 1);
    Path empty;
    empty.AddRegularPolygon({}, 2, 10.0f, 0.0f);
    CHECK(empty.Empty());
    path.AddRegularPolygon({}, 3, 2.0f, 0.0f);
    FlattenPath(path, {}, 0.1f, points, contours);
    REQUIRE(contours.size() == 2);
    CHECK(contours[1].count == 3);
    CHECK(contours[1].closed);
}

TEST_CASE("Engine.Tessellator.SliversStayOnTheirOutline", "[engine][paint]") {
    // A snow cap along a ridge that thins to nothing at both ends: the anti-aliased edge used to turn its thin parts
    // inside out, and the triangulation then drew a wedge far from the cap.
    std::vector<Float2> cap;
    for (int i = 0; i <= 40; ++i) {
        const float x = static_cast<float>(i) * 5.0f;
        cap.push_back({x, 40.0f + 15.0f * std::sin(x * 0.05f)});
    }
    for (int i = 40; i >= 0; --i) {
        const float x = static_cast<float>(i) * 5.0f;
        const float thickness = 3.0f * std::pow(std::sin(Pi * x / 200.0f), 4.0f);
        cap.push_back({x, 40.0f + 15.0f * std::sin(x * 0.05f) + thickness});
    }
    MeshBuffer mesh;
    FillPolygon(mesh, cap, Opaque, 1.0f);
    REQUIRE_FALSE(mesh.indices.empty());
    for (const Float2 centroid : Centroids(mesh))
        CHECK((InsidePolygon(centroid, cap) || DistanceToPolygon(centroid, cap) <= 1.0f));
}

TEST_CASE("Engine.ShapeArea.FillRulesDecideHolesAndOverlaps", "[engine][paint]") {
    const auto outer = Square({0.0f, 0.0f}, 30.0f);
    const auto inner = Square({10.0f, 10.0f}, 10.0f);
    const auto innerReversed = Square({10.0f, 10.0f}, 10.0f, false);
    CHECK(FilledArea({outer, inner}, FillRule::EvenOdd) == Approx(800.0f).epsilon(0.01));
    CHECK(FilledArea({outer, innerReversed}, FillRule::EvenOdd) == Approx(800.0f).epsilon(0.01));
    CHECK(FilledArea({outer, inner}, FillRule::NonZero) == Approx(900.0f).epsilon(0.01));
    CHECK(FilledArea({outer, innerReversed}, FillRule::NonZero) == Approx(800.0f).epsilon(0.01));
    // Overlapping contours: NonZero merges them, EvenOdd leaves their overlap empty.
    const auto a = Square({0.0f, 0.0f}, 10.0f);
    const auto b = Square({5.0f, 5.0f}, 10.0f);
    CHECK(FilledArea({a, b}, FillRule::NonZero) == Approx(175.0f).epsilon(0.01));
    CHECK(FilledArea({a, b}, FillRule::EvenOdd) == Approx(150.0f).epsilon(0.01));
    // An island inside a hole is filled again.
    CHECK(FilledArea({outer, inner, Square({13.0f, 13.0f}, 4.0f)}, FillRule::EvenOdd) == Approx(816.0f).epsilon(0.01));
}

TEST_CASE("Engine.ShapeArea.SelfIntersectingOutlinesFollowTheRule", "[engine][paint]") {
    std::vector<Float2> star;
    for (int i = 0; i < 5; ++i) {
        const float angle = -Pi * 0.5f + static_cast<float>(i) * 4.0f * Pi / 5.0f;
        star.push_back({50.0f * std::cos(angle), 50.0f * std::sin(angle)});
    }
    const float nonZero = FilledArea({star}, FillRule::NonZero);
    const float evenOdd = FilledArea({star}, FillRule::EvenOdd);
    // The pentagon in the middle is wound twice: filled under NonZero, empty under EvenOdd.
    const float pentagonRadius = 50.0f * std::cos(2.0f * Pi / 5.0f) / std::cos(Pi / 5.0f);
    const float pentagon = 2.5f * pentagonRadius * pentagonRadius * std::sin(2.0f * Pi / 5.0f);
    CHECK(nonZero - evenOdd == Approx(pentagon).epsilon(0.01));
}

TEST_CASE("Engine.ShapeArea.GlowBandIsRoundAndFades", "[engine][paint]") {
    const auto square = Square({0.0f, 0.0f}, 10.0f);
    const Contour contour{0, 4, true};
    ShapeArea area;
    area.Set(square, std::span(&contour, 1), FillRule::NonZero, Unit);
    Regions edge;
    area.Get(edge);
    Regions band;
    area.Band(4.0f, StrokeJoin::Round, 1.0f, 0.25f, band);
    MeshBuffer mesh;
    FadeRegions(mesh, band, edge, 4.0f, Opaque);
    REQUIRE_FALSE(mesh.indices.empty());
    float farthest = 0.0f;
    for (const PaintVertex& vertex : mesh.vertices) {
        const float distance = DistanceToPolygon({vertex.x, vertex.y}, square);
        farthest = std::max(farthest, distance);
        // No miter spikes at the corners, and the colour fades with the distance from the edge.
        CHECK(distance <= 4.0f + 0.01f);
        CHECK(std::abs(static_cast<float>(vertex.color.a) - 255.0f * (1.0f - distance / 4.0f)) <= 2.0f);
    }
    CHECK(farthest == Approx(4.0f).margin(0.01f));
    // Sides plus quarter circles; the corners are chords within the 0.25 tolerance, so slightly smaller.
    CHECK(TriangleArea(mesh) == Approx(4.0f * 10.0f * 4.0f + Pi * 16.0f).epsilon(0.03));
}

TEST_CASE("Engine.ShapeArea.InsideBandsCoverThinPartsWhole", "[engine][paint]") {
    const auto drawBand = [](const std::vector<Float2>& outline, float distance) {
        const Contour contour{0, static_cast<std::uint32_t>(outline.size()), true};
        ShapeArea area;
        area.Set(outline, std::span(&contour, 1), FillRule::NonZero, Unit);
        Regions band;
        area.Band(distance, StrokeJoin::Miter, 4.0f, 0.25f, band);
        MeshBuffer mesh;
        FillRegions(mesh, band, Opaque, 0.001f);
        return CoveredArea(mesh);
    };
    const auto square = Square({0.0f, 0.0f}, 10.0f);
    CHECK(drawBand(square, -2.0f) == Approx(100.0f - 36.0f).epsilon(0.01));
    CHECK(drawBand(square, 2.0f) == Approx(196.0f - 100.0f).epsilon(0.01));
    const std::vector<Float2> sliver{{0.0f, 0.0f}, {20.0f, 0.0f}, {20.0f, 1.0f}, {0.0f, 1.0f}};
    CHECK(drawBand(sliver, -2.0f) == Approx(20.0f).epsilon(0.01));
}

TEST_CASE("Engine.Tessellator.TriangulationKeepsHolesEmpty", "[engine][paint]") {
    const auto outer = Square({0.0f, 0.0f}, 30.0f);
    const auto hole = Square({10.0f, 10.0f}, 10.0f);
    Regions regions;
    regions.points = outer;
    regions.points.insert(regions.points.end(), hole.begin(), hole.end());
    regions.rings = {{0, 4, true}, {4, 4, true}};
    regions.ends = {2};
    MeshBuffer mesh;
    for (int pass = 0; pass < 2; ++pass) { // The triangulator's memory is reused between calls
        mesh.Clear();
        FillRegions(mesh, regions, Opaque, 1.0f);
        // Opaque core: the outline half a fringe in, the hole half a fringe out.
        CHECK(CoveredArea(mesh) == Approx(29.0f * 29.0f - 11.0f * 11.0f).epsilon(0.01));
        for (const Float2 centroid : Centroids(mesh))
            CHECK_FALSE((centroid.x > 10.6f && centroid.x < 19.4f && centroid.y > 10.6f && centroid.y < 19.4f));
    }
}
