#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <vector>
#include "Graphics/Paint/Internals/Tessellator.h"
#include "Graphics/Paint/Path.h"
#include "Utilities/Affine2.h"
#include "Utilities/Color4.h"
#include "Utilities/FloatRect.h"
#include "Utilities/MathUtils.h"

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
