#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Diagnostics/DevelopmentChecks.h"
#include "Graphics/Light/Debug/LightGuides.h"
#include "Graphics/Light/LightLayer.h"
#include "Tests/PaintHarness.h"

using Catch::Approx;

namespace {
constexpr Color4 Dark{20, 30, 40, 255};
constexpr Float2 Size{400.0f, 200.0f};

// The light images filled so far: passes cleared to an opaque colour (the harness clears its canvas to transparent).
std::vector<const NullCommand*> ImagePasses(const PaintHarness& h) {
    std::vector<const NullCommand*> passes;
    for (const NullCommand* pass : h.device.CommandsOf(NullCommandType::BeginPass))
        if (pass->pass.clear && pass->pass.clear->a > 0.0f) passes.push_back(pass);
    return passes;
}

std::vector<GpuBlend> Blends(const PaintHarness& h) {
    std::vector<GpuBlend> blends;
    for (const NullCommand* command : h.device.CommandsOf(NullCommandType::SetPipeline))
        blends.push_back(command->blend);
    return blends;
}
} // namespace

TEST_CASE("Engine.Light.IsAQuadCarryingItsShape", "[engine][paint][light]") {
    PaintHarness h;
    h.Begin();
    h.paint.Light({.at = {100.0f, 50.0f},
                   .radius = 40.0f,
                   .color = {200, 100, 0, 255},
                   .intensity = 0.5f,
                   .core = 0.25f,
                   .cone = {.direction = Pi / 2.0f, .halfAngle = Pi / 3.0f, .softEdge = Pi / 6.0f}});
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 6);
    for (const PaintVertex& vertex : vertices) {
        CHECK(vertex.kind == PaintKind::Light);
        CHECK(vertex.color == PaintColor{100, 50, 0, 0}); // Scaled by the intensity; alpha 0 adds up
        CHECK(vertex.shape0 == Approx(40.0f));
        CHECK(vertex.shape1 == Approx(10.0f));
        CHECK(vertex.shape2 == Approx(0.5f));                 // Cosine of 60 degrees
        CHECK(vertex.shape3 == Approx(std::sqrt(3.0f) / 2.0f)); // Cosine of 30 degrees
        CHECK(vertex.style0 == Approx(0.0f).margin(1e-5));
        CHECK(vertex.style1 == Approx(1.0f));
        // The quad covers the radius; the shader's local coordinates start at the light.
        CHECK(vertex.x - vertex.u == Approx(100.0f));
        CHECK(vertex.y - vertex.v == Approx(50.0f));
        CHECK(std::abs(vertex.u) >= 40.0f);
    }
}

TEST_CASE("Engine.Light.WithoutAConeHasNoConeTest", "[engine][paint][light]") {
    PaintHarness h;
    h.Begin();
    h.paint.Light({.at = {100.0f, 50.0f}, .radius = 40.0f});
    h.paint.Light({.at = {100.0f, 50.0f}, .radius = 40.0f, .cone = {.halfAngle = 2.0f * Pi}});
    h.End();
    for (const PaintVertex& vertex : h.Drawn())
        CHECK(vertex.shape2 < -1.0f); // The shader's sign for the whole circle
}

TEST_CASE("Engine.Light.IgnoresTheGlowQualitySetting", "[engine][paint][light]") {
    PaintHarness h;
    h.context.frame.quality.glow = GlowQuality::Off;
    h.Begin();
    h.paint.Glow({10.0f, 10.0f}, 5.0f, {});
    REQUIRE(h.list.Vertices().empty());
    h.paint.Light({.at = {10.0f, 10.0f}, .radius = 5.0f});
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 6);
    CHECK(vertices[0].shape0 == Approx(5.0f));
    CHECK(vertices[0].color == PaintColor{255, 255, 255, 0});
}

TEST_CASE("Engine.Light.FollowsTransformsAndOpacity", "[engine][paint][light]") {
    PaintHarness h;
    h.Begin();
    {
        auto camera = h.paint.Transform(Affine2::Translate({10.0f, 20.0f}) * Affine2::Scale(2.0f));
        auto half = h.paint.Opacity(0.5f);
        h.paint.Light({.at = {5.0f, 5.0f}, .radius = 10.0f});
    }
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 6);
    CHECK(vertices[0].shape0 == Approx(10.0f)); // In the light's own units: the shader needs no zoom
    CHECK(vertices[0].color == PaintColor{128, 128, 128, 0});
    float lowest = std::numeric_limits<float>::max();
    for (const PaintVertex& vertex : vertices)
        lowest = std::min(lowest, vertex.x);
    CHECK(lowest <= 10.0f + 2.0f * (5.0f - 10.0f)); // Scaled by 2 around the moved origin
}

TEST_CASE("Engine.Light.VisiblePolygonIsAFanAroundTheLight", "[engine][paint][light]") {
    PaintHarness h;
    const std::array<Float2, 4> visible{Float2{80.0f, 30.0f}, {120.0f, 30.0f}, {120.0f, 70.0f}, {80.0f, 70.0f}};
    h.Begin();
    h.paint.Light({.at = {100.0f, 50.0f}, .radius = 40.0f}, visible);
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 12); // Four triangles, the rim closed
    for (std::size_t i = 0; i < vertices.size(); i += 3) {
        CHECK(vertices[i].x == Approx(100.0f));
        CHECK(vertices[i].y == Approx(50.0f));
        CHECK(vertices[i].u == Approx(0.0f));
        CHECK(vertices[i].kind == PaintKind::Light);
    }
    CHECK(vertices[1].u == Approx(-20.0f));
    CHECK(vertices[1].v == Approx(-20.0f));
    CHECK(vertices[11].x == Approx(80.0f)); // The last triangle returns to the first point
    CHECK(vertices[11].y == Approx(30.0f));
}

TEST_CASE("Engine.Light.DegenerateInputDrawsNothing", "[engine][paint][light]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    PaintHarness h;
    const std::array<Float2, 2> tooFew{Float2{1.0f, 1.0f}, {2.0f, 2.0f}};
    const std::array<Float2, 3> notFinite{Float2{1.0f, 1.0f}, {2.0f, 2.0f}, {nan, 2.0f}};
    h.Begin();
    h.paint.Light({.at = {nan, 0.0f}, .radius = 10.0f});
    h.paint.Light({.radius = 0.0f});
    h.paint.Light({.radius = nan});
    h.paint.Light({.radius = 10.0f, .color = {255, 255, 255, 0}});
    h.paint.Light({.radius = 10.0f, .intensity = 0.0f});
    h.paint.Light({.radius = 10.0f, .intensity = nan});
    h.paint.Light({.radius = 10.0f}, tooFew);
    h.paint.Light({.radius = 10.0f}, notFinite);
    CHECK(h.list.Vertices().empty());
    h.End();
}

TEST_CASE("Engine.Light.ConeAndCoreValuesAreKeptInRange", "[engine][paint][light]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    PaintHarness h;
    h.Begin();
    h.paint.Light({.radius = 10.0f, .core = 2.0f, .cone = {.halfAngle = 0.0f}});
    h.paint.Light({.radius = 10.0f, .core = nan, .cone = {.halfAngle = 1.0f, .softEdge = 5.0f}});
    h.paint.Light({.radius = 10.0f, .cone = {.halfAngle = nan}});
    h.End();
    const auto vertices = h.Drawn();
    REQUIRE(vertices.size() == 18);
    CHECK(vertices[0].shape1 == Approx(10.0f));
    CHECK(vertices[0].shape2 < vertices[0].shape3); // The shader's smoothstep needs two different edges
    CHECK(vertices[6].shape1 == Approx(0.0f));
    CHECK(vertices[6].shape2 == Approx(std::cos(1.0f)));
    CHECK(vertices[6].shape3 == Approx(1.0f)); // The soft edge is at most the whole cone
    CHECK(vertices[12].shape2 < -1.0f);        // An unusable cone is the whole circle
}

TEST_CASE("Engine.LightLayer.DrawsLightsIntoAnImageAndMultiplies", "[engine][paint][light]") {
    PaintHarness h(Size, 2.0f);
    LightLayer layer;
    const std::array lights{LightSource{.at = {100.0f, 50.0f}, .radius = 40.0f}};
    h.Begin();
    layer.Draw(h.paint, {.ambient = Dark, .lights = lights});
    h.End();

    const auto passes = ImagePasses(h);
    REQUIRE(passes.size() == 1);
    const NullCommand* image = passes[0];
    CHECK(image->pass.viewSize == Float2{200.0f, 100.0f}); // Half the target by default
    CHECK(image->pass.clear->r == Approx(20.0f / 255.0f));
    CHECK(image->pass.clear->b == Approx(40.0f / 255.0f));
    CHECK(image->pass.clear->a == Approx(1.0f));

    const auto blends = Blends(h);
    REQUIRE(blends.size() == 2);
    CHECK(blends[0] == GpuBlend::Premultiplied);
    CHECK(blends[1] == GpuBlend::Multiply);

    const auto draws = h.Draws();
    REQUIRE(draws.size() == 2);
    const auto light = h.Drawn(0);
    REQUIRE(light.size() == 6);
    CHECK(light[0].kind == PaintKind::Light);
    CHECK(light[0].x - light[0].u * 0.5f == Approx(50.0f)); // Scaled with the image
    CHECK(light[0].y - light[0].v * 0.5f == Approx(25.0f));
    const auto stamp = h.Drawn(1);
    REQUIRE(stamp.size() == 6);
    float right = 0.0f;
    float bottom = 0.0f;
    for (const PaintVertex& vertex : stamp) {
        CHECK(vertex.kind == PaintKind::Texture);
        CHECK(vertex.color == PaintColor{255, 255, 255, 255});
        right = std::max(right, vertex.x);
        bottom = std::max(bottom, vertex.y);
    }
    CHECK(right == Approx(Size.x)); // Covers the whole target
    CHECK(bottom == Approx(Size.y));
}

TEST_CASE("Engine.LightLayer.ResolutionSetsTheImageSize", "[engine][paint][light]") {
    const auto viewSize = [](float resolution) {
        PaintHarness h(Size, 1.0f);
        LightLayer layer({.resolution = resolution});
        h.Begin();
        layer.Draw(h.paint, {.ambient = Dark});
        h.End();
        const auto passes = ImagePasses(h);
        return passes.empty() ? Float2{} : passes[0]->pass.viewSize;
    };
    CHECK(viewSize(1.0f) == Size);
    CHECK(viewSize(0.25f) == Float2{100.0f, 50.0f});
    CHECK(viewSize(0.01f) == Float2{100.0f, 50.0f});
    CHECK(viewSize(9.0f) == Size);
    CHECK(viewSize(std::numeric_limits<float>::quiet_NaN()) == Float2{200.0f, 100.0f});
}

TEST_CASE("Engine.LightLayer.UsesTheViewForLightsNotForTheStamp", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer({.resolution = 1.0f});
    const std::array lights{LightSource{.at = {1000.0f, 1000.0f}, .radius = 40.0f}};
    const CameraView view{.offset = {200.0f, 100.0f}, .target = {1000.0f, 1000.0f}, .zoom = 2.0f};
    h.Begin();
    layer.Draw(h.paint, {.ambient = Dark, .lights = lights}, view);
    h.End();
    const auto light = h.Drawn(0);
    REQUIRE(light.size() == 6);
    CHECK(light[0].x - light[0].u * 2.0f == Approx(200.0f));
    CHECK(light[0].y - light[0].v * 2.0f == Approx(100.0f));
    float right = 0.0f;
    for (const PaintVertex& vertex : h.Drawn(1))
        right = std::max(right, vertex.x);
    CHECK(right == Approx(Size.x));
}

TEST_CASE("Engine.LightLayer.ViewportLimitsBothSteps", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer({.resolution = 1.0f});
    const std::array lights{LightSource{.at = {50.0f, 50.0f}, .radius = 40.0f}};
    const CameraView left{.viewport = {0.0f, 0.0f, 200.0f, 200.0f}};
    const CameraView right{.viewport = {200.0f, 0.0f, 200.0f, 200.0f}};
    h.Begin();
    layer.Draw(h.paint, {.ambient = Dark, .lights = lights}, left);
    layer.Draw(h.paint, {.ambient = Dark, .lights = lights}, right);
    h.End();
    // A split screen: each view fills the image again, and stamps it only on its own part.
    CHECK(ImagePasses(h).size() == 2);
    std::vector<PixelRect> scissors;
    for (const NullCommand* command : h.device.CommandsOf(NullCommandType::SetScissor))
        if (command->scissor) scissors.push_back(*command->scissor);
    CHECK(std::find(scissors.begin(), scissors.end(), PixelRect{0, 0, 200, 200}) != scissors.end());
    CHECK(std::find(scissors.begin(), scissors.end(), PixelRect{200, 0, 200, 200}) != scissors.end());
}

TEST_CASE("Engine.LightLayer.WhiteAmbientDrawsNothing", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer;
    const std::array lights{LightSource{.at = {50.0f, 50.0f}, .radius = 40.0f}};
    h.Begin();
    layer.Draw(h.paint, {.ambient = {255, 255, 255, 255}, .lights = lights});
    layer.Draw(h.paint, {.ambient = {255, 255, 255, 0}});
    CHECK(h.list.Vertices().empty());
    h.End();
    CHECK(ImagePasses(h).empty());
}

TEST_CASE("Engine.LightLayer.IgnoresAmbientAlpha", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer;
    h.Begin();
    layer.Draw(h.paint, {.ambient = {20, 30, 40, 0}});
    h.End();
    const auto passes = ImagePasses(h);
    REQUIRE(passes.size() == 1);
    CHECK(passes[0]->pass.clear->a == Approx(1.0f));
    CHECK(passes[0]->pass.clear->r == Approx(20.0f / 255.0f));
}

TEST_CASE("Engine.LightLayer.RefusesToDrawInsideACamera", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer;
    h.Begin();
    {
        auto camera = h.paint.Camera({.offset = {10.0f, 0.0f}});
        if (DevelopmentChecks()) {
            CHECK_THROWS_AS(layer.Draw(h.paint, {.ambient = Dark}), std::invalid_argument);
        } else {
            layer.Draw(h.paint, {.ambient = Dark});
            CHECK(h.list.Rejected() == 1);
        }
    }
    CHECK(h.list.Vertices().empty());
    h.End();
}

TEST_CASE("Engine.LightLayer.FollowsTheTargetSize", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    LightLayer layer({.resolution = 1.0f});
    Surface other({100.0f, 50.0f});
    h.Begin();
    layer.Draw(h.paint, {.ambient = Dark});
    {
        auto into = h.paint.Target(other);
        layer.Draw(h.paint, {.ambient = Dark});
    }
    h.End();
    const auto passes = ImagePasses(h);
    REQUIRE(passes.size() == 2);
    CHECK(passes[0]->pass.viewSize == Size);
    CHECK(passes[1]->pass.viewSize == Float2{100.0f, 50.0f});
}

TEST_CASE("Engine.LightGuides.DrawInCanvasUnitsWithoutChangingTheFrame", "[engine][paint][light]") {
    PaintHarness h(Size, 1.0f);
    const std::array lights{LightSource{.at = {50.0f, 50.0f}, .radius = 40.0f, .core = 0.5f},
                            LightSource{.at = {150.0f, 50.0f}, .radius = 40.0f, .cone = {.halfAngle = 0.5f}},
                            LightSource{.at = {250.0f, 50.0f}, .radius = 0.0f}};
    h.Begin();
    DrawLightGuides(h.paint, {.ambient = Dark, .lights = lights}, {.zoom = 2.0f});
    CHECK_FALSE(h.list.Vertices().empty());
    CHECK(h.paint.CurrentTransform() == Affine2::Identity());
    h.End();
    CHECK(h.list.Rejected() == 0);
}
