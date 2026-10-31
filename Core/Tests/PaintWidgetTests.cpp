#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <stdexcept>
#include <vector>
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Draw/Neon.h"
#include "Graphics/Icons/Icons.h"
#include "Graphics/Paint/Path.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"
#include "Tests/PaintHarness.h"
#include "Utilities/MathUtils.h"

using Catch::Approx;

namespace {
constexpr Color4 Cyan{40, 220, 255, 255};
constexpr Color4 Dark{12, 10, 30, 255};
constexpr float Margin = 2.0f; // Anti-aliasing fringe around shapes, in drawing units

const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

constexpr NeonFrameStyle Frame{.outerWidth = 4.0f,
                               .innerWidth = 2.0f,
                               .outerAlpha = 0.1f,
                               .innerAlpha = 0.3f,
                               .coreWidth = 0.4f,
                               .coreMinWidth = 1.0f,
                               .coreWhiten = 0.5f};
constexpr ArcadeButtonStyle Button{.body = Dark,
                                   .shadow = Dark,
                                   .depth = 8.0f,
                                   .notch = 10.0f,
                                   .grow = 0.1f,
                                   .squash = 0.05f,
                                   .frameWidth = 2.0f,
                                   .appearDrop = 50.0f,
                                   .frame = Frame};

// Every drawn vertex of the frame (all draw calls).
std::vector<PaintVertex> AllVertices(const PaintHarness& h) {
    std::vector<PaintVertex> vertices;
    for (std::size_t i = 0; i < h.Draws().size(); ++i) {
        const auto drawn = h.Drawn(i);
        vertices.insert(vertices.end(), drawn.begin(), drawn.end());
    }
    return vertices;
}

std::size_t CountKind(const std::vector<PaintVertex>& vertices, PaintKind kind) {
    return static_cast<std::size_t>(
        std::count_if(vertices.begin(), vertices.end(), [kind](const PaintVertex& v) { return v.kind == kind; }));
}

bool Inside(const std::vector<PaintVertex>& vertices, const FloatRect& area) {
    const FloatRect bounds = area.Inset(-Margin);
    return std::all_of(vertices.begin(), vertices.end(), [&](const PaintVertex& v) {
        return v.x >= bounds.x && v.x <= bounds.Right() && v.y >= bounds.y && v.y <= bounds.Bottom();
    });
}

std::size_t FrameShapes(GlowQuality quality) {
    PaintHarness h;
    h.context.frame.quality.glow = quality;
    h.Begin();
    DrawNeonFrame(h.paint, {20.0f, 20.0f, 120.0f, 60.0f}, 8.0f, 2.0f, Cyan, 1.0f, Frame);
    h.End();
    return CountKind(AllVertices(h), PaintKind::ChamferRect);
}
} // namespace

TEST_CASE("Engine.Widgets.NeonFrameBandsFollowGlowQuality", "[engine][paint]") {
    const std::size_t full = FrameShapes(GlowQuality::Full);
    const std::size_t reduced = FrameShapes(GlowQuality::Reduced);
    const std::size_t off = FrameShapes(GlowQuality::Off);
    CHECK(full > reduced);
    CHECK(reduced > off);
    CHECK(off > 0);

    PaintHarness h;
    h.Begin();
    DrawNeonFrame(h.paint, {20.0f, 20.0f, 120.0f, 60.0f}, 8.0f, 2.0f, Cyan, 1.0f, Frame);
    h.End();
    CHECK(h.Draws().size() == 1); // Additive bands share the frame's draw call
}

TEST_CASE("Engine.Widgets.BandsMatchArcsAndFullTurnsMatchRings", "[engine][paint]") {
    const Float2 center{100.0f, 90.0f};
    const LineStyle style{
        .width = 99.0f, .color = Cyan, .cap = StrokeCap::Round, .glow = {4.0f, Cyan}, .emission = 0.4f};
    for (const float sweep : {1.3f, -1.3f, 2.0f * Pi, 3.0f * Pi}) {
        PaintHarness band;
        PaintHarness arc;
        band.Begin();
        DrawBand(band.paint, center, 20.0f, 30.0f, 0.7f, sweep, style);
        band.End();
        arc.Begin();
        LineStyle expected = style;
        expected.width = 10.0f;
        arc.paint.Arc(center, 25.0f, 0.7f, sweep, expected);
        arc.End();
        const auto actual = AllVertices(band);
        const auto reference = AllVertices(arc);
        REQUIRE(actual.size() == reference.size());
        REQUIRE_FALSE(actual.empty());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            CHECK(actual[i].x == reference[i].x);
            CHECK(actual[i].y == reference[i].y);
            CHECK(actual[i].u == reference[i].u);
            CHECK(actual[i].v == reference[i].v);
            CHECK(actual[i].shape0 == reference[i].shape0);
            CHECK(actual[i].shape1 == reference[i].shape1);
            CHECK(actual[i].shape2 == reference[i].shape2);
            CHECK(actual[i].style0 == reference[i].style0);
            CHECK(actual[i].style1 == reference[i].style1);
            CHECK(actual[i].color == reference[i].color);
            CHECK(actual[i].flags == reference[i].flags);
            CHECK(actual[i].emission == reference[i].emission);
            CHECK(actual[i].kind == reference[i].kind);
        }
        if (std::abs(sweep) >= 2.0f * Pi) CHECK(CountKind(actual, PaintKind::Circle) == actual.size());
    }
    PaintHarness empty;
    empty.Begin();
    DrawBand(empty.paint, center, 20.0f, 20.0f, 0.0f, Pi, style);
    DrawBand(empty.paint, center, 30.0f, 20.0f, 0.0f, Pi, style);
    DrawRing(empty.paint, center, 20.0f, 20.0f, Outline(1.0f, Cyan));
    empty.End();
    CHECK(empty.Draws().empty());
}

TEST_CASE("Engine.Widgets.RingsMatchCircleOutlines", "[engine][paint]") {
    PaintHarness ring;
    PaintHarness circle;
    ring.Begin();
    DrawRing(ring.paint, {80.0f, 90.0f}, 20.0f, 30.0f, Outline(99.0f, Cyan));
    ring.End();
    circle.Begin();
    circle.paint.Circle({80.0f, 90.0f}, 25.0f, Outline(10.0f, Cyan));
    circle.End();
    const auto actual = AllVertices(ring);
    const auto expected = AllVertices(circle);
    REQUIRE(actual.size() == expected.size());
    REQUIRE_FALSE(actual.empty());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        CHECK(actual[i].x == expected[i].x);
        CHECK(actual[i].y == expected[i].y);
        CHECK(actual[i].shape0 == expected[i].shape0);
        CHECK(actual[i].style0 == expected[i].style0);
        CHECK(actual[i].color == expected[i].color);
        CHECK(actual[i].kind == PaintKind::Circle);
    }
}

TEST_CASE("Engine.Painter.PolygonAndPathGradientsKeepLocalCoordinatesAndFringeCoverage", "[engine][paint]") {
    const std::vector<Float2> points{{20.0f, 20.0f}, {100.0f, 20.0f}, {100.0f, 80.0f}, {20.0f, 80.0f}};
    const Affine2 transform =
        Affine2::Translate({120.0f, 60.0f}) * Affine2::Rotate(0.4f) * Affine2::Scale({1.5f, 0.7f});
    for (const bool pathFill : {false, true}) {
        for (const Gradient gradient : {Gradient::Linear({-40.0f, 0.0f}, {40.0f, 0.0f}, {0, 0, 0, 0}, Cyan),
                                        Gradient::Radial(50.0f, {0, 0, 0, 0}, Cyan)}) {
            PaintHarness h;
            h.Begin();
            {
                auto moved = h.paint.Transform(transform);
                auto faded = h.paint.Opacity(0.5f);
                if (pathFill) {
                    Path path;
                    path.AddPolygon(points);
                    h.paint.FillPath(path, {.gradient = gradient, .emission = 0.5f});
                } else {
                    h.paint.Polygon(points, {.gradient = gradient, .emission = 0.5f});
                }
            }
            h.End();
            const auto vertices = AllVertices(h);
            REQUIRE_FALSE(vertices.empty()); // A transparent 'from' must not suppress the fill.
            std::size_t core = 0;
            std::size_t fringe = 0;
            for (const PaintVertex& v : vertices) {
                const Float2 local = transform.Inverse().Apply({v.x, v.y}) - Float2{60.0f, 50.0f};
                CHECK(v.u == Approx(local.x).margin(1e-4f));
                CHECK(v.v == Approx(local.y).margin(1e-4f));
                CHECK(v.color == PaintColor{});
                CHECK(v.color2.a == 128);
                CHECK(v.emission == 128);
                CHECK((v.flags & PaintCoverage) != 0);
                if (gradient.kind == GradientKind::Linear) {
                    CHECK((v.flags & PaintGradientLinear) != 0);
                    CHECK(v.style0 == Approx((local.x + 40.0f) / 80.0f).margin(1e-5f));
                } else {
                    CHECK((v.flags & PaintGradientRadial) != 0);
                    CHECK(v.style0 == Approx(1.0f / 50.0f));
                }
                if (v.shape3 == 1.0f)
                    ++core;
                else if (v.shape3 == 0.0f)
                    ++fringe;
                else
                    FAIL("Fringe endpoints must carry independent coverage");
            }
            CHECK(core > 0);
            CHECK(fringe > 0);
        }
    }
}

TEST_CASE("Engine.Painter.PathGradientUsesOneBoundsAcrossContours", "[engine][paint]") {
    Path path;
    path.AddRegularPolygon({30.0f, 30.0f}, 4, 10.0f, 0.0f);
    path.AddRegularPolygon({90.0f, 30.0f}, 4, 10.0f, 0.0f);
    PaintHarness h;
    h.Begin();
    h.paint.FillPath(path, {.gradient = Gradient::Vertical(20.0f, Cyan, Dark)});
    h.End();
    const auto vertices = AllVertices(h);
    REQUIRE_FALSE(vertices.empty());
    for (const auto& v : vertices) {
        CHECK(v.u == Approx(v.x - 60.0f).margin(1e-4f));
        CHECK(v.v == Approx(v.y - 30.0f).margin(1e-4f));
    }
}

TEST_CASE("Engine.Painter.PolygonAndPathBordersRespectJoinAndKeepMiterDefault", "[engine][paint]") {
    const std::vector<Float2> points{{30.0f, 30.0f}, {100.0f, 30.0f}, {30.0f, 100.0f}};
    std::vector<std::size_t> counts;
    for (const StrokeJoin join : {StrokeJoin::Miter, StrokeJoin::Bevel, StrokeJoin::Round}) {
        PaintHarness polygon;
        PaintHarness pathFill;
        PaintHarness line;
        polygon.Begin();
        ShapeStyle style = Outline(10.0f, Cyan);
        if (join != StrokeJoin::Miter) style.stroke.join = join;
        polygon.paint.Polygon(points, style);
        polygon.End();
        pathFill.Begin();
        Path path;
        path.AddPolygon(points);
        pathFill.paint.FillPath(path, style);
        pathFill.End();
        line.Begin();
        line.paint.Polyline(points, {.width = 10.0f, .color = Cyan, .join = join}, true);
        line.End();
        const auto actual = AllVertices(polygon);
        const auto filled = AllVertices(pathFill);
        const auto expected = AllVertices(line);
        REQUIRE_FALSE(actual.empty());
        REQUIRE(actual.size() == expected.size());
        REQUIRE(filled.size() == expected.size());
        counts.push_back(actual.size());
        for (std::size_t i = 0; i < actual.size(); ++i) {
            CHECK(actual[i].x == expected[i].x);
            CHECK(actual[i].y == expected[i].y);
            CHECK(actual[i].color == expected[i].color);
            CHECK(filled[i].x == expected[i].x);
            CHECK(filled[i].y == expected[i].y);
        }
    }
    CHECK(counts[0] != counts[1]);
    CHECK(counts[0] != counts[2]);
}

TEST_CASE("Engine.Widgets.ArcadeButtonBodyFollowsItsLook", "[engine][paint]") {
    const FloatRect rect{100.0f, 60.0f, 160.0f, 50.0f};
    PaintHarness h;
    h.Begin();
    CHECK(DrawArcadeButtonBody(h.paint, rect, {.appear = 0.0f}, 0.0f, Button).Empty());
    const FloatRect idle = DrawArcadeButtonBody(h.paint, rect, {}, 0.0f, Button);
    const FloatRect selected = DrawArcadeButtonBody(h.paint, rect, {.animation = {.highlight = 1.0f}}, 0.0f, Button);
    const FloatRect pressed = DrawArcadeButtonBody(h.paint, rect, {.animation = {.press = 1.0f}}, 0.0f, Button);
    const FloatRect moved = DrawArcadeButtonBody(h.paint, rect, {.offset = {30.0f, 0.0f}}, 0.0f, Button);
    h.End();

    CHECK(idle.Size() == rect.Size());
    CHECK(idle.Center().x == Approx(rect.Center().x));
    CHECK(selected.width > idle.width);
    CHECK(pressed.width < idle.width);
    CHECK(pressed.Center().y > idle.Center().y); // Sinks towards its shadow
    CHECK(moved.Center().x == Approx(idle.Center().x + 30.0f));
}

TEST_CASE("Engine.Widgets.ProgressBarStaysInsideItsFrameAndFillDrivesTheGlow", "[engine][paint]") {
    const FloatRect bar{40.0f, 80.0f, 240.0f, 20.0f};
    const ProgressBarStyle style{.ghost = Cyan,
                                 .glowAlpha = 0.3f,
                                 .glowSpread = 6.0f,
                                 .segment = 20.0f,
                                 .segmentGap = 3.0f,
                                 .frame = Cyan,
                                 .frameOutset = 4.0f,
                                 .frameWidth = 2.0f,
                                 .frameStyle = Frame};
    const auto draw = [&](float fill) {
        PaintHarness h;
        h.Begin();
        DrawProgressBar(h.paint, bar, {.fill = fill, .ghost = fill + 0.1f, .color = Cyan}, style);
        h.End();
        return AllVertices(h);
    };
    const auto empty = draw(0.0f);
    const auto half = draw(0.5f);
    const float frameReach = style.frameOutset + style.frameWidth * Frame.outerWidth;
    CHECK(Inside(half, bar.Inset(-frameReach)));
    CHECK(half.size() > empty.size());
}

TEST_CASE("Engine.Widgets.BackgroundLayersStayInsideAndFollowQuality", "[engine][paint]") {
    const FloatRect area{10.0f, 10.0f, 200.0f, 120.0f};
    PaintHarness h;
    h.Begin();
    CHECK_THROWS_AS(DrawGrid2D(h.paint, area, 0.0f, {}, Cyan, 1.0f), std::invalid_argument);
    CHECK_THROWS_AS(DrawScanlines(h.paint, area, Cyan, -1.0f, 1.0f), std::invalid_argument);
    DrawGrid2D(h.paint, area, 25.0f, {7.0f, -13.0f}, Cyan, 1.0f);
    DrawVignette(h.paint, area, 40.0f, Dark);
    DrawScanlines(h.paint, area, Dark, 4.0f, 1.0f);
    h.End();
    CHECK(Inside(AllVertices(h), area));

    PaintHarness quiet;
    quiet.context.frame.quality.backgroundEffects = false;
    quiet.Begin();
    DrawScanlines(quiet.paint, area, Dark, 4.0f, 1.0f);
    quiet.End();
    CHECK(quiet.Draws().empty());
}

TEST_CASE("Engine.Widgets.NeonTextGlowsOnlyWhenAsked", "[engine][paint]") {
    const NeonTextStyle neon{.haloRadius = 4.0f, .haloAlpha = 0.5f, .coreWhiten = 0.3f};
    const auto halos = [&](float glow) {
        PaintHarness h;
        h.context.font = &TestFont();
        h.Begin();
        DrawNeonText(h.paint, "PLAY", {100.0f, 40.0f}, {.size = 24.0f, .color = Cyan, .align = TextAlign::Center}, glow,
                     neon);
        h.End();
        const auto vertices = AllVertices(h);
        CHECK(CountKind(vertices, PaintKind::Glyph) > 0);
        return CountKind(vertices, PaintKind::GlyphHalo);
    };
    CHECK(halos(1.0f) > 0);
    CHECK(halos(0.0f) == 0);
}

TEST_CASE("Engine.Widgets.ChipAndButtonLabelsUseTheGamesFont", "[engine][paint]") {
    PaintHarness h;
    h.context.font = &TestFont();
    h.Begin();
    DrawNeonChip(h.paint, "NEW", {100.0f, 50.0f}, 14.0f, Cyan, {.text = Dark, .paddingX = 6.0f, .notch = 4.0f});
    DrawArcadeButton(h.paint, {40.0f, 100.0f, 160.0f, 50.0f}, "START", 20.0f, {}, 0.0f, Button);
    h.End();
    CHECK(CountKind(AllVertices(h), PaintKind::Glyph) > 0);
}

TEST_CASE("Engine.Widgets.ShapesProduceGeometryAndLightningGlowFollowsQuality", "[engine][paint]") {
    const LightningStyle bolt{
        .segments = 6, .jitter = 10.0f, .width = 4.0f, .coreWidth = 1.5f, .glowRadius = 20.0f, .glowAlpha = 0.5f};
    const auto glows = [&](GlowQuality quality) {
        PaintHarness h;
        h.context.frame.quality.glow = quality;
        h.Begin();
        DrawLightning(h.paint, {50.0f, 10.0f}, {60.0f, 150.0f}, 3, Cyan, 1.0f, bolt);
        h.End();
        return CountKind(AllVertices(h), PaintKind::Glow);
    };
    CHECK(glows(GlowQuality::Full) > 0);
    CHECK(glows(GlowQuality::Off) == 0);

    PaintHarness h;
    h.Begin();
    DrawStar(h.paint, {40.0f, 40.0f}, 20.0f, Fill(Cyan));
    DrawDiamond(h.paint, {160.0f, 40.0f}, {10.0f, 16.0f}, {.fill = Cyan, .stroke = {2.0f, Dark}});
    DrawRotatedRect(h.paint, {200.0f, 40.0f}, {12.0f, 4.0f}, Pi / 4.0f, Fill(Cyan));
    DrawTabbedPanelOutline(h.paint, {20.0f, 100.0f, 200.0f, 80.0f}, {40.0f, 80.0f, 60.0f, 20.0f}, 2.0f, Cyan);
    h.End();
    CHECK(Inside(AllVertices(h), {0.0f, 0.0f, 240.0f, 190.0f}));
    CHECK(h.Draws().size() == 1);
}

TEST_CASE("Engine.Widgets.EveryIconDrawsInsideItsBoxInOneBatch", "[engine][paint]") {
    constexpr Float2 Center{100.0f, 100.0f};
    constexpr float Size = 20.0f;
    constexpr IconStyle Style{.color = Cyan, .detail = Dark, .line = 2.0f};
    for (std::size_t i = 0; i < IconCount; ++i) {
        CAPTURE(i);
        PaintHarness h;
        h.Begin();
        DrawIcon(h.paint, static_cast<Icon>(i), Center, Size, Style);
        h.End();
        REQUIRE(h.Draws().size() == 1);
        const auto vertices = h.Drawn();
        CHECK_FALSE(vertices.empty());
        CHECK(Inside(vertices, FloatRect::FromCenter(Center, Float2{Size, Size} * 2.5f)));
    }
}

TEST_CASE("Engine.Widgets.RowSlotsCentreTheRow", "[engine][paint]") {
    CHECK(RowSlot({100.0f, 20.0f}, 3, 1, 10.0f).x == 100.0f);
    CHECK(RowSlot({100.0f, 20.0f}, 2, 0, 10.0f).x == 95.0f);
    CHECK(RowSlot({100.0f, 20.0f}, 2, 1, 10.0f).y == 20.0f);
}
