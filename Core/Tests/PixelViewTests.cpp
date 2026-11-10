#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <vector>
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/PixelView/PixelView.h"
#include "Graphics/Surface/Surface.h"
#include "Stage/Stage.h"
#include "Tests/PaintHarness.h"
#include "Tests/Support/TestSceneServices.h"

using Catch::Approx;

namespace {
constexpr FloatRect Canvas{0.0f, 0.0f, 1280.0f, 720.0f};
constexpr float Art = 3.0f;

CameraView ViewAt(Float2 target, float zoom = 1.0f) {
    return {.offset = Canvas.Center(), .target = target, .zoom = zoom, .viewport = Canvas};
}

bool Whole(float value, float tolerance = 1e-3f) { return std::abs(value - std::round(value)) < tolerance; }

// The pixel of the image a canvas-coordinate point inside it falls on (fractional).
Float2 ImagePixel(const PixelViewLayout& layout, Float2 inside) {
    const float perUnit = static_cast<float>(layout.pixels.x) / layout.area.width;
    return {(inside.x - layout.area.x) * perUnit, (inside.y - layout.area.y) * perUnit};
}

std::vector<const NullCommand*> Passes(const PaintHarness& h) {
    return h.device.CommandsOf(NullCommandType::BeginPass);
}

bool AllHard(const std::vector<PaintVertex>& vertices) {
    for (const PaintVertex& vertex : vertices)
        if ((vertex.flags & PaintHard) == 0) return false;
    return !vertices.empty();
}
} // namespace

TEST_CASE("Engine.PixelView.ScaleIsWholeDevicePixelsPerArtPixel", "[engine][pixel]") {
    const PixelViewLayout plain = PixelView::Plan(ViewAt({0.0f, 0.0f}), Canvas, {1.0f, 1.0f}, Art);
    CHECK(plain.scale == 3);
    CHECK(plain.shown.zoom == Approx(1.0f));
    CHECK(plain.pixels.x == static_cast<int>(std::ceil(1280.0f / 3.0f)) + 2);
    CHECK(plain.pixels.y == 240 + 2);

    // A 1920x1080 window: 1.5 device pixels per unit, 4.5 per art pixel, rounded to 5; the zoom follows.
    const PixelViewLayout big = PixelView::Plan(ViewAt({0.0f, 0.0f}), Canvas, {1.5f, 1.5f}, Art);
    CHECK(big.scale == 5);
    CHECK(big.shown.zoom == Approx(5.0f / 4.5f));
    CHECK(big.pixels.x == static_cast<int>(std::ceil(1920.0f / 5.0f)) + 2);

    // Zoom moves in whole steps and never below one device pixel per art pixel.
    CHECK(PixelView::Plan(ViewAt({}, 1.4f), Canvas, {1.0f, 1.0f}, Art).scale == 4);
    CHECK(PixelView::Plan(ViewAt({}, 1.4f), Canvas, {1.0f, 1.0f}, Art).shown.zoom == Approx(4.0f / 3.0f));
    CHECK(PixelView::Plan(ViewAt({}, 0.05f), Canvas, {1.0f, 1.0f}, Art).scale == 1);
    CHECK(PixelView::Plan(ViewAt({}, 0.0f), Canvas, {1.0f, 1.0f}, Art).scale == 3); // An invalid zoom counts as 1
}

TEST_CASE("Engine.PixelView.TargetSnapsToTheArtGridAndTheImageToDevicePixels", "[engine][pixel]") {
    for (const Float2 device : {Float2{1.0f, 1.0f}, Float2{1366.0f / 1280.0f, 768.0f / 720.0f}, Float2{0.75f, 0.75f}}) {
        for (const float x : {0.0f, 1.2f, 1.6f, 100.49f, -37.7f, 5000.3f}) {
            const PixelViewLayout layout = PixelView::Plan(ViewAt({x, x * 0.5f}), Canvas, device, Art);
            CHECK(Whole(layout.shown.target.x / Art));
            CHECK(Whole(layout.shown.target.y / Art));
            // The image's corner sits on a device pixel, and the image covers the viewport.
            CHECK(Whole(layout.dest.x * device.x));
            CHECK(Whole(layout.dest.y * device.y));
            CHECK(layout.dest.x <= Canvas.x + 1e-3f);
            CHECK(layout.dest.y <= Canvas.y + 1e-3f);
            CHECK(layout.dest.Right() >= Canvas.Right() - 1e-3f);
            CHECK(layout.dest.Bottom() >= Canvas.Bottom() - 1e-3f);
            CHECK(layout.dest.width * device.x == Approx(static_cast<float>(layout.pixels.x * layout.scale)));
            // Every point of the art grid lands on a whole pixel of the image, one art pixel apart.
            const Float2 grid{layout.shown.target.x + 7.0f * Art, layout.shown.target.y - 4.0f * Art};
            const Float2 at = ImagePixel(layout, layout.shown.ToScreen(grid));
            CHECK(Whole(at.x, 2e-3f));
            CHECK(Whole(at.y, 2e-3f));
            const Float2 next = ImagePixel(layout, layout.shown.ToScreen(grid + Float2{Art, Art}));
            CHECK(next.x - at.x == Approx(1.0f).margin(1e-3));
            CHECK(next.y - at.y == Approx(1.0f).margin(1e-3));
        }
    }
}

TEST_CASE("Engine.PixelView.CameraScrollsByDevicePixelsAcrossGridSteps", "[engine][pixel]") {
    // At 1.5 device pixels per unit an art pixel is 5 device pixels: a slow camera moves the image a pixel at a time,
    // and a world point never jumps, also where the target snaps to the next grid point.
    const Float2 device{1.5f, 1.5f};
    const Float2 world{400.0f, 300.0f};
    float previous = PixelView::Plan(ViewAt({0.0f, 0.0f}), Canvas, device, Art).ToCanvas(world).x * device.x;
    for (int step = 1; step <= 120; ++step) {
        const float x = static_cast<float>(step) * 0.1f;
        const PixelViewLayout layout = PixelView::Plan(ViewAt({x, 0.0f}), Canvas, device, Art);
        const float now = layout.ToCanvas(world).x * device.x;
        CHECK(previous - now >= -1e-3f); // The camera moves right: the world moves left
        CHECK(previous - now <= 1.0f + 1e-3f);
        previous = now;
    }
    // After 12 units (18 device pixels at a quantized zoom of 5/4.5) the world moved by about that much.
    const float start = PixelView::Plan(ViewAt({0.0f, 0.0f}), Canvas, device, Art).ToCanvas(world).x * device.x;
    CHECK(start - previous == Approx(12.0f * 5.0f / 3.0f).margin(1.0f));
}

TEST_CASE("Engine.PixelView.PickingFollowsWhatIsShown", "[engine][pixel]") {
    for (const Float2 device : {Float2{1.0f, 1.0f}, Float2{1366.0f / 1280.0f, 768.0f / 720.0f}}) {
        const PixelViewLayout layout = PixelView::Plan(ViewAt({123.4f, -56.7f}, 1.3f), Canvas, device, Art);
        for (const Float2 world : {Float2{123.4f, -56.7f}, Float2{0.0f, 0.0f}, Float2{300.0f, 100.0f}}) {
            const Float2 canvas = layout.ToCanvas(world);
            const Float2 back = layout.ToWorld(canvas);
            CHECK(back.x == Approx(world.x).margin(1e-2));
            CHECK(back.y == Approx(world.y).margin(1e-2));
        }
        // The camera's target shows within one device pixel of where the camera wanted it.
        const Float2 middle = layout.ToCanvas({123.4f, -56.7f});
        CHECK(std::abs(middle.x - Canvas.Center().x) * device.x <= 1.0f + 1e-3f);
        CHECK(std::abs(middle.y - Canvas.Center().y) * device.y <= 1.0f + 1e-3f);
    }
}

TEST_CASE("Engine.PixelView.DrawsHardEdgesIntoAPixelImageAndPresentsIt", "[engine][pixel]") {
    PaintHarness h({1280.0f, 720.0f}, 1.5f);
    h.context.artPixel = 3;
    PixelView pixels;
    h.Begin();
    {
        auto into = pixels.Begin(h.paint, ViewAt({10.0f, 20.0f}));
        CHECK(h.paint.HardEdges());
        CHECK(h.paint.Bounds() == pixels.Layout().area);
        auto world = h.paint.Camera(pixels.Shown());
        h.paint.Circle({10.0f, 20.0f}, 30.0f, Fill({255, 0, 0, 255}));
        h.paint.Polygon(std::array{Float2{0.0f, 0.0f}, Float2{40.0f, 5.0f}, Float2{10.0f, 30.0f}},
                        Fill({0, 255, 0, 255}));
        h.paint.Line({0.0f, 0.0f}, {50.0f, 10.0f}, {.width = 0.3f, .color = {0, 0, 255, 255}});
    }
    CHECK_FALSE(h.paint.HardEdges());
    pixels.Present(h.paint);
    h.End();

    const auto passes = Passes(h);
    REQUIRE(passes.size() >= 2);
    const NullCommand* image = passes[1];
    const PixelViewLayout& layout = pixels.Layout();
    CHECK(image->pass.viewport.width == layout.pixels.x);
    CHECK(image->pass.viewport.height == layout.pixels.y);
    CHECK(image->pass.viewOrigin == layout.area.Position());
    CHECK(image->pass.viewSize == layout.area.Size());
    const GpuTargetDesc& target = h.device.Describe(image->pass.target);
    CHECK(target.filter == GpuFilter::Nearest);
    CHECK(target.width == layout.pixels.x);

    const auto drawn = h.Drawn(0);
    CHECK(AllHard(drawn));
    // Tessellated edges keep almost no fringe, and a line thinner than a pixel keeps its colour.
    bool faded = false;
    for (const PaintVertex& vertex : drawn)
        if (vertex.kind == PaintKind::Solid && vertex.color.a != 0 && vertex.color.a != 255) faded = true;
    CHECK_FALSE(faded);
    // The image lands on the canvas where the layout says, not hard itself.
    const auto shown = h.Drawn(h.Draws().size() - 1);
    REQUIRE_FALSE(shown.empty());
    CHECK((shown[0].flags & PaintHard) == 0);
    float left = shown[0].x;
    for (const PaintVertex& vertex : shown)
        left = std::min(left, vertex.x);
    CHECK(left == Approx(layout.dest.x).margin(1e-3));
}

TEST_CASE("Engine.PixelView.SurfacesInsideFollowItsResolution", "[engine][pixel][light]") {
    PaintHarness h({1280.0f, 720.0f}, 2.0f);
    h.context.artPixel = 4;
    PixelView pixels;
    LightLayer light; // Automatic resolution: one light value per art pixel in a pixel view
    Surface plain({100.0f, 50.0f});
    h.Begin();
    {
        auto into = pixels.Begin(h.paint, ViewAt({}));
        CHECK(h.paint.Stale(plain));
        {
            auto inner = h.paint.Target(plain);
        }
        light.Draw(h.paint, {.ambient = {30, 30, 30, 255}});
    }
    h.End();
    const PixelViewLayout& layout = pixels.Layout();
    CHECK(layout.scale == 8);
    const auto passes = Passes(h);
    REQUIRE(passes.size() >= 4);
    // 100 x 50 canvas units at 2 device pixels per unit and 8 per art pixel: 25 x 13 pixels.
    CHECK(passes[2]->pass.viewport.width == 25);
    CHECK(passes[2]->pass.viewport.height == 13);
    // The light image has the view's own pixels.
    CHECK(passes[3]->pass.viewport.width == layout.pixels.x);
    CHECK(passes[3]->pass.viewport.height == layout.pixels.y);
}

TEST_CASE("Engine.PixelView.PixelSurfacesHaveTheirSizeInPixels", "[engine][pixel]") {
    PaintHarness h({400.0f, 200.0f}, 2.5f);
    Surface pixels({37.2f, 10.0f}, {.pixels = true});
    CHECK(pixels.Size() == Float2{38.0f, 10.0f});
    h.Begin();
    {
        auto into = h.paint.Target(pixels);
        CHECK(h.paint.HardEdges());
        CHECK(h.paint.Bounds() == FloatRect{0.0f, 0.0f, 38.0f, 10.0f});
        CHECK(h.paint.TargetPixels() == Float2{38.0f, 10.0f});
        h.paint.Rect({2.0f, 2.0f, 5.0f, 5.0f}, Fill({255, 255, 255, 255}));
    }
    h.paint.Image(pixels, {0.0f, 0.0f, 38.0f, 10.0f});
    h.End();
    const auto passes = Passes(h);
    REQUIRE(passes.size() == 3); // The canvas, the surface, the canvas again
    CHECK(passes[1]->pass.viewport.width == 38);
    CHECK(h.device.Describe(passes[1]->pass.target).filter == GpuFilter::Nearest);
    CHECK(AllHard(h.Drawn(0)));
    CHECK_THROWS_AS(Surface({0.0f, 1.0f}, {.pixels = true}), std::invalid_argument);
}

TEST_CASE("Engine.PixelView.SpritePixelsAreArtPixels", "[engine][pixel]") {
    PaintHarness h({400.0f, 200.0f}, 1.0f);
    h.context.artPixel = 3;
    PixelCanvas canvas(4, 2);
    canvas.Set(0, 0, 1);
    const PixelSprite sprite(canvas, {0, 0});
    const PixelPalette palette{Color4{255, 0, 0, 255}};
    CHECK(h.paint.ArtPixel() == 3.0f);
    CHECK(h.paint.PixelWorld());
    CHECK_FALSE(h.paint.PixelUi());
    h.Begin();
    h.paint.Sprite(sprite, palette, {10.0f, 20.0f});
    h.paint.Sprite(sprite, palette, {10.0f, 50.0f}, {.scale = 2.0f});
    h.End();
    const auto drawn = h.Drawn(0);
    REQUIRE(drawn.size() == 12);
    CHECK(drawn[2].x - drawn[0].x == Approx(12.0f)); // 4 sprite pixels of 3 units
    CHECK(drawn[2].y - drawn[0].y == Approx(6.0f));
    CHECK(drawn[8].x - drawn[6].x == Approx(24.0f));
    h.context.artPixel = 0; // A smooth project: one unit per art pixel
    CHECK(h.paint.ArtPixel() == 1.0f);
    CHECK_FALSE(h.paint.PixelWorld());
}

namespace {
class PixelStageView final : public StageView {
public:
    void DrawWorld(Painter& paint, const StagePass& pass) const override {
        worldHard = paint.HardEdges();
        seen = pass.view;
        paint.Circle(pass.view.target, 20.0f, Fill({255, 255, 255, 255}));
    }
    void DrawHud(Painter& paint) const override {
        hudHard = paint.HardEdges();
        paint.Rect({10.0f, 10.0f, 50.0f, 20.0f}, Fill({255, 255, 0, 255}));
    }
    mutable bool worldHard = false;
    mutable bool hudHard = false;
    mutable CameraView seen;
};

struct StageRun {
    bool worldHard;
    bool hudHard;
    std::size_t passes;
};

StageRun RunStage(int artPixel, bool pixelUi, bool pixelWorld = true) {
    TestSceneServices services;
    Stage stage(services.services, {.pixelWorld = pixelWorld});
    stage.Cameras().Main().SetPose({.center = {100.3f, 50.8f}});
    PixelStageView view;
    PaintHarness h({1280.0f, 720.0f}, 1.0f);
    h.context.artPixel = artPixel;
    h.context.pixelUi = pixelUi;
    h.Begin();
    stage.Draw(h.paint, view);
    h.End();
    CHECK(h.list.Rejected() == 0);
    if (artPixel > 0 && pixelWorld) {
        const CameraView shown = stage.Shown();
        CHECK(Whole(shown.target.x / static_cast<float>(artPixel)));
        CHECK(shown.target == view.seen.target);
        const Float2 picked = stage.ToWorld(shown.ToScreen({200.0f, 80.0f}));
        CHECK(picked.x == Approx(200.0f).margin(0.05));
        CHECK(picked.y == Approx(80.0f).margin(0.05));
    } else {
        CHECK(stage.Shown().target == Float2{100.3f, 50.8f});
    }
    return {view.worldHard, view.hudHard, Passes(h).size()};
}
} // namespace

TEST_CASE("Engine.Stage.PixelProjectsDrawTheWorldThroughAPixelView", "[engine][stage][pixel]") {
    const StageRun smooth = RunStage(0, false);
    CHECK_FALSE(smooth.worldHard);
    CHECK(smooth.passes == 1);

    const StageRun pixel = RunStage(3, false);
    CHECK(pixel.worldHard);
    CHECK_FALSE(pixel.hudHard); // The HUD stays sharp by default
    CHECK(pixel.passes == 3);   // The canvas, the view's image, the canvas again

    const StageRun pixelUi = RunStage(3, true);
    CHECK(pixelUi.worldHard);
    CHECK(pixelUi.hudHard);
    CHECK(pixelUi.passes == 5); // And the HUD's image

    const StageRun off = RunStage(3, true, false); // StageSettings::pixelWorld turns both off
    CHECK_FALSE(off.worldHard);
    CHECK_FALSE(off.hudHard);
    CHECK(off.passes == 1);
}
