#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <optional>
#include <stdexcept>
#include "Graphics/Paint/Path.hpp"
#include "Graphics/Surface/Surface.hpp"
#include "Tests/PaintHarness.h"
#include "Utilities/MathUtils.h"

using Catch::Approx;

namespace {
constexpr Color4 Red{255, 0, 0, 255};
constexpr Color4 Blue{0, 0, 255, 255};

bool HasPositiveArea(const std::vector<PaintVertex>& vertices) {
    for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
        const Float2 a{vertices[i].x, vertices[i].y};
        const Float2 b{vertices[i + 1].x, vertices[i + 1].y};
        const Float2 c{vertices[i + 2].x, vertices[i + 2].y};
        if (std::abs(Cross(b - a, c - a)) > 1e-6f) return true;
    }
    return false;
}

struct FakeLegacy final : LegacyHost {
    void Bind(const LegacyBinding& binding) override {
        log.push_back("bind");
        last = binding;
    }
    void Flush() override { log.push_back("flush"); }
    std::vector<std::string> log;
    LegacyBinding last;
};
} // namespace

TEST_CASE("Engine.Paint.NormalAdditiveAndGlowShareOneDrawCall", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    h.paint.Circle({50.0f, 50.0f}, 10.0f, {.fill = Red, .stroke = {2.0f, Blue}, .glow = {12.0f, Red}});
    h.paint.Glow({100.0f, 50.0f}, 30.0f, {.color = Blue});
    {
        auto add = h.paint.Blending(Blend::Add);
        h.paint.Rect({10.0f, 10.0f, 20.0f, 20.0f}, Fill(Red));
    }
    h.paint.Line({0.0f, 0.0f}, {100.0f, 100.0f}, {.width = 3.0f, .cap = StrokeCap::Round});
    h.End();
    REQUIRE(h.Draws().size() == 1);
    CHECK(h.device.CommandsOf(NullCommandType::SetPipeline).size() == 1);
}

TEST_CASE("Engine.Paint.AdditiveColorsHaveZeroAlphaAndOpacityScalesPremultiplied", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    {
        auto half = h.paint.Opacity(0.5f);
        h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
        auto add = h.paint.Blending(Blend::Add);
        h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
    }
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 12);
    CHECK(vertices[0].color == PaintColor{128, 0, 0, 128});
    CHECK(vertices[6].color == PaintColor{128, 0, 0, 0});
}

TEST_CASE("Engine.Paint.ClipIntersectsAndSplitsBatchesWithPixelScissors", "[engine][paint]") {
    PaintHarness h({400.0f, 200.0f}, 2.0f);
    h.Begin();
    h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
    {
        auto outer = h.paint.Clip({10.0f, 20.0f, 100.0f, 50.0f});
        auto inner = h.paint.Clip({50.25f, 0.0f, 500.0f, 500.0f});
        h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
    }
    h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
    h.End();
    CHECK(h.Draws().size() == 3);
    const auto scissors = h.device.CommandsOf(NullCommandType::SetScissor);
    REQUIRE(scissors.size() == 3);
    CHECK_FALSE(scissors[0]->scissor.has_value());
    REQUIRE(scissors[1]->scissor.has_value());
    CHECK(*scissors[1]->scissor == PixelRect{100, 40, 120, 100}); // x 50.25..110, y 20..70 at 2x
    CHECK_FALSE(scissors[2]->scissor.has_value());
}

TEST_CASE("Engine.Paint.ClipUnderRotationIsRejected", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    {
        auto rotated = h.paint.Transform(Affine2::Rotate(0.3f));
        CHECK_THROWS_AS(h.paint.Clip({0.0f, 0.0f, 10.0f, 10.0f}), std::invalid_argument);
    }
    h.End();
}

TEST_CASE("Engine.Paint.TransformsComposeInnerFirst", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    {
        auto move = h.paint.Transform(Affine2::Translate({100.0f, 0.0f}));
        auto scale = h.paint.Transform(Affine2::Scale(2.0f));
        h.paint.Glow({10.0f, 10.0f}, 5.0f, {});
    }
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 6);
    float minX = 1e9f;
    float maxX = -1e9f;
    for (const PaintVertex& vertex : vertices) {
        minX = std::min(minX, vertex.x);
        maxX = std::max(maxX, vertex.x);
    }
    CHECK((minX + maxX) * 0.5f == Approx(120.0f)); // Centre (10, 10) scaled, then moved
    CHECK(maxX - minX > 20.0f);                    // Radius 5 doubled, plus anti-aliasing margin
}

// PaintScope cannot be moved, so user code always ends scopes in order; the recorder still guards the stack.
TEST_CASE("Engine.Paint.ScopesEndedOutOfOrderFailTheFrame", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    const std::uint32_t outer = h.list.PushScope(ScopeKind::Opacity);
    const std::uint32_t inner = h.list.PushScope(ScopeKind::Clip);
    h.list.PopScope(outer);
    h.list.PopScope(inner); // Already closed with the outer scope
    CHECK(h.list.Depth() == 0);
    CHECK_THROWS_AS(h.list.Finish(), std::logic_error);
}

TEST_CASE("Engine.Paint.OpenScopeAtFrameEndFails", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    (void)h.list.PushScope(ScopeKind::Transform);
    CHECK_THROWS_AS(h.list.Finish(), std::logic_error);
}

TEST_CASE("Engine.Paint.MoreTexturesThanSlotsSplitTheBatch", "[engine][paint]") {
    PaintHarness h;
    std::vector<Surface> surfaces;
    for (std::size_t i = 0; i < PaintTextureSlots + 1; ++i)
        surfaces.emplace_back(Float2{8.0f, 8.0f});
    h.Begin();
    for (Surface& surface : surfaces) {
        auto into = h.paint.Target(surface);
    }
    h.device.ClearCommands();
    for (const Surface& surface : surfaces)
        h.paint.Image(surface, {0.0f, 0.0f, 8.0f, 8.0f});
    h.End();
    REQUIRE(h.Draws().size() == 2);
    const auto binds = h.device.CommandsOf(NullCommandType::BindTextures);
    REQUIRE(binds.size() == 2);
    CHECK(binds[0]->textures.size() == PaintTextureSlots);
    CHECK(binds[1]->textures.size() == 1);
}

TEST_CASE("Engine.Paint.SurfacesFollowTheCanvasScale", "[engine][paint]") {
    PaintHarness h({400.0f, 200.0f}, 2.0f);
    Surface surface({100.0f, 50.0f});
    h.Begin();
    CHECK(h.paint.Stale(surface));
    {
        auto into = h.paint.Target(surface, Red);
        CHECK(h.paint.Bounds() == FloatRect{0.0f, 0.0f, 100.0f, 50.0f});
        CHECK(h.paint.PixelScale() == Approx(2.0f));
        h.paint.Circle({50.0f, 25.0f}, 10.0f, Fill(Blue));
    }
    CHECK_FALSE(h.paint.Stale(surface));
    CHECK(surface.Texture().Width() == 200);
    CHECK(surface.Texture().Height() == 100);
    h.End();
    const auto passes = h.device.CommandsOf(NullCommandType::BeginPass);
    REQUIRE(passes.size() == 3); // Canvas, surface, canvas resumed
    CHECK(passes[1]->pass.clear.has_value());
    CHECK_FALSE(passes[2]->pass.clear.has_value());

    h.Begin(3.0f);
    CHECK(h.paint.Stale(surface)); // New resolution: the image must be drawn again
    h.End();
}

TEST_CASE("Engine.Paint.DestroyedSurfaceIsFreedAfterTheFrame", "[engine][paint]") {
    PaintHarness h;
    const std::size_t before = h.device.LiveTargets();
    h.Begin();
    {
        Surface surface({10.0f, 10.0f});
        {
            auto into = h.paint.Target(surface);
            h.paint.Rect({0.0f, 0.0f, 5.0f, 5.0f}, Fill(Red));
        }
        h.paint.Image(surface, {0.0f, 0.0f, 10.0f, 10.0f});
    }
    CHECK(h.device.LiveTargets() == before + 1); // Still needed by the recorded image draw
    h.End();
    CHECK(h.device.LiveTargets() == before);
}

TEST_CASE("Engine.Paint.GroupsCompositeOnceAndReusePooledImages", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    for (int i = 0; i < 2; ++i) {
        auto group = h.paint.Group({.opacity = 0.5f});
        h.paint.Circle({20.0f, 20.0f}, 10.0f, Fill(Red));
        h.paint.Circle({25.0f, 20.0f}, 10.0f, Fill(Red));
    }
    h.End();
    CHECK(h.resources.PooledImages() == 1);
    const auto draws = h.Draws();
    REQUIRE(draws.size() == 4); // Group contents, composite, contents, composite
    const auto composite = h.device.DrawnVertices(*draws[1]);
    REQUIRE(composite.size() == 6);
    CHECK(composite[0].kind == PaintKind::Texture);
    CHECK(composite[0].color.a == 128);
}

TEST_CASE("Engine.Paint.LegacyDrawingKeepsOrderWithoutRedundantSwitches", "[engine][paint]") {
    FakeLegacy legacy;
    PaintHarness h({400.0f, 200.0f}, 2.0f, &legacy);
    h.Begin();
    h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Red));
    {
        auto a = h.paint.Legacy();
    }
    {
        auto b = h.paint.Legacy(); // Nothing drawn in between: no hand-over
    }
    CHECK(legacy.log == std::vector<std::string>{"bind"});
    CHECK(h.Draws().size() == 1); // The rectangle executed before raylib drew
    h.paint.Rect({0.0f, 0.0f, 10.0f, 10.0f}, Fill(Blue));
    {
        auto moved = h.paint.Transform(Affine2::Translate({5.0f, 0.0f}));
        auto c = h.paint.Legacy();
        CHECK(legacy.last.transform.tx == Approx(5.0f));
    }
    CHECK(legacy.log == std::vector<std::string>{"bind", "flush", "bind"});
    h.End();
    CHECK(legacy.log.back() == "flush"); // Raylib's drawing goes before the frame closes
    CHECK(h.context.legacySwitches == 2);
}

TEST_CASE("Engine.Paint.InvalidStylesThrowDegenerateShapesDrawNothing", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    CHECK_THROWS_AS(h.paint.Circle({0.0f, 0.0f}, 5.0f, {.stroke = {-1.0f, Red}}), std::invalid_argument);
    CHECK_THROWS_AS(h.paint.Line({0.0f, 0.0f}, {1.0f, 1.0f}, {.width = -2.0f}), std::invalid_argument);
    h.paint.Circle({0.0f, 0.0f}, 0.0f, Fill(Red));
    h.paint.Rect({0.0f, 0.0f, 0.0f, 10.0f}, Fill(Red));
    h.paint.Circle({std::nanf(""), 0.0f}, 3.0f, Fill(Red));
    h.paint.Polygon(std::vector<Float2>{{0.0f, 0.0f}, {1.0f, 1.0f}}, Fill(Red));
    h.End();
    CHECK(h.Draws().empty());
}

TEST_CASE("Engine.Paint.GlowQualityReducesOrRemovesGlows", "[engine][paint]") {
    PaintHarness h;
    h.context.frame.quality.glow = GlowQuality::Off;
    h.Begin();
    h.paint.Glow({10.0f, 10.0f}, 5.0f, {});
    h.paint.Circle({10.0f, 10.0f}, 5.0f, {.glow = {10.0f, Red}});
    h.End();
    CHECK(h.Draws().empty());
}

TEST_CASE("Engine.Paint.PolylinesAndPathsProduceSolidGeometry", "[engine][paint]") {
    PaintHarness h;
    h.Begin();
    const std::vector<Float2> zigzag{{0.0f, 0.0f}, {20.0f, 10.0f}, {40.0f, 0.0f}, {60.0f, 10.0f}};
    h.paint.Polyline(zigzag, {.width = 4.0f, .color = Red, .cap = StrokeCap::Round, .join = StrokeJoin::Round});
    Path path;
    path.MoveTo({0.0f, 0.0f}).QuadTo({20.0f, 40.0f}, {40.0f, 0.0f}).Close();
    h.paint.FillPath(path, {.fill = Blue, .stroke = {1.0f, Red}});
    h.End();
    REQUIRE(h.Draws().size() == 1);
    const auto vertices = h.Drawn();
    CHECK(HasPositiveArea(vertices));
    for (const PaintVertex& vertex : vertices)
        CHECK(vertex.kind == PaintKind::Solid);
}

TEST_CASE("Engine.Paint.ArcQuadsCoverTheirSectorOnly", "[engine][paint]") {
    constexpr Float2 Center{100.0f, 100.0f};
    constexpr float Radius = 50.0f;
    const auto quadOf = [&](float sweep, StrokeCap cap) {
        PaintHarness h;
        h.Begin();
        h.paint.Arc(Center, Radius, 0.0f, sweep, {.width = 4.0f, .color = Red, .cap = cap});
        h.End();
        return h.Drawn();
    };
    const auto covers = [](const std::vector<PaintVertex>& quad, Float2 point) {
        for (std::size_t i = 0; i + 2 < quad.size(); i += 3) {
            const Float2 a{quad[i].x, quad[i].y};
            const Float2 b{quad[i + 1].x, quad[i + 1].y};
            const Float2 c{quad[i + 2].x, quad[i + 2].y};
            const float ab = Cross(b - a, point - a);
            const float bc = Cross(c - b, point - b);
            const float ca = Cross(a - c, point - c);
            if ((ab >= 0.0f && bc >= 0.0f && ca >= 0.0f) || (ab <= 0.0f && bc <= 0.0f && ca <= 0.0f)) return true;
        }
        return false;
    };
    const auto area = [](const std::vector<PaintVertex>& quad) {
        float total = 0.0f;
        for (std::size_t i = 0; i + 2 < quad.size(); i += 3)
            total += std::abs(Cross(Float2{quad[i + 1].x - quad[i].x, quad[i + 1].y - quad[i].y},
                                    Float2{quad[i + 2].x - quad[i].x, quad[i + 2].y - quad[i].y})) *
                     0.5f;
        return total;
    };
    const float circleBox = (2.0f * Radius + 4.0f) * (2.0f * Radius + 4.0f);
    for (const float sweep : {0.5f * Pi, -0.5f * Pi, 1.5f * Pi}) {
        for (const StrokeCap cap : {StrokeCap::Butt, StrokeCap::Round}) {
            const auto quad = quadOf(sweep, cap);
            CHECK(covers(quad, Center + Float2::FromAngle(0.0f, Radius + 2.0f)));
            CHECK(covers(quad, Center + Float2::FromAngle(sweep * 0.5f, Radius - 2.0f)));
            CHECK(covers(quad, Center + Float2::FromAngle(sweep, Radius)));
            if (cap == StrokeCap::Round)
                CHECK(covers(quad, Center + Float2::FromAngle(sweep, Radius) +
                                       Float2::FromAngle(sweep + (sweep > 0 ? 0.5f : -0.5f) * Pi, 1.9f)));
            if (std::abs(sweep) <= 0.5f * Pi) CHECK(area(quad) < 0.5f * circleBox);
        }
    }
}
