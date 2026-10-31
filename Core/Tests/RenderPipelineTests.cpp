#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "Graphics/Gpu/Internals/NullDevice.h"
#include "Graphics/Render/Internals/RenderPipeline.h"
#include "Graphics/Text/FontFace.h"

using Catch::Approx;

namespace {
RenderPipelineSettings Settings(PostSettings post = {}) {
    RenderPipelineSettings settings;
    settings.canvasSize = {400.0f, 200.0f};
    settings.background = {10, 20, 30, 255};
    settings.letterbox = {0, 0, 0, 255};
    settings.post = post;
    return settings;
}

void Frame(RenderPipeline& pipeline, Vec2 window) {
    Painter& paint = pipeline.BeginFrame(window, 1.0, 0.016f);
    paint.Circle({200.0f, 100.0f}, 20.0f, Fill({255, 0, 0, 255}));
    (void)pipeline.FinishCanvas();
    (void)pipeline.BeginWindow();
    pipeline.EndFrame();
}
} // namespace

TEST_CASE("Engine.RenderPipeline.LetterboxKeepsAspectOnWholePixels", "[engine][paint][render]") {
    NullDevice device;
    const RenderPipeline pipeline(device, Settings());
    CHECK(pipeline.Letterbox({800, 400}) == FloatRect{0.0f, 0.0f, 800.0f, 400.0f});
    CHECK(pipeline.Letterbox({1000, 400}) == FloatRect{100.0f, 0.0f, 800.0f, 400.0f});
    CHECK(pipeline.Letterbox({801, 601}) == FloatRect{0.0f, 100.0f, 801.0f, 401.0f}); // Odd sizes: centred, whole
    CHECK(pipeline.ToCanvas({500, 200}, {1000, 400}) == Vec2{200, 100});
    CHECK_FALSE(pipeline.ToCanvas({50, 200}, {1000, 400}).has_value()); // In the letterbox
}

TEST_CASE("Engine.RenderPipeline.LayoutCountersDescribeOnlyTheCompletedFrame", "[engine][paint][render][text]") {
    const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    const auto frame = [&](bool repeat) {
        Painter& paint = pipeline.BeginFrame({400, 200}, 1.0, 0.016f);
        paint.Text(font, "Label", {10.0f, 10.0f}, {.size = 20.0f});
        if (repeat) paint.Text(font, "Label", {10.0f, 40.0f}, {.size = 20.0f});
        (void)pipeline.FinishCanvas();
        Painter& window = pipeline.BeginWindow();
        window.Text(font, "Overlay", {10.0f, 10.0f}, {.size = 20.0f});
        pipeline.EndFrame();
    };
    frame(true);
    CHECK(pipeline.LastFrame().layoutHits == 1);
    CHECK(pipeline.LastFrame().layoutMisses == 2);
    frame(false);
    CHECK(pipeline.LastFrame().layoutHits == 2);
    CHECK(pipeline.LastFrame().layoutMisses == 0);
    Frame(pipeline, {400, 200});
    CHECK(pipeline.LastFrame().layoutHits == 0);
    CHECK(pipeline.LastFrame().layoutMisses == 0);
}

TEST_CASE("Engine.RenderPipeline.CanvasFollowsTheWindowAndTheHeightLimit", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    Frame(pipeline, {1600, 800});
    const auto canvasPass = device.CommandsOf(NullCommandType::BeginPass).front();
    CHECK(canvasPass->pass.viewport == PixelRect{0, 0, 1600, 800}); // Upscaled: window resolution
    CHECK(device.Describe(canvasPass->pass.target).height == 800);

    pipeline.SetMaxRenderHeight(500);
    device.ClearCommands();
    Frame(pipeline, {1600, 800});
    CHECK(device.CommandsOf(NullCommandType::BeginPass).front()->pass.viewport == PixelRect{0, 0, 1000, 500});

    device.ClearCommands();
    Frame(pipeline, {200, 100}); // Smaller window: drawn at the logical size and filtered down
    CHECK(device.CommandsOf(NullCommandType::BeginPass).front()->pass.viewport == PixelRect{0, 0, 400, 200});
}

TEST_CASE("Engine.RenderPipeline.WindowDrawingExecutesBeforeToolOverlays", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    (void)pipeline.BeginFrame({400, 200}, 1.0, 0.016f);
    (void)pipeline.FinishCanvas();
    Painter& window = pipeline.BeginWindow();
    const std::size_t presented = device.CommandsOf(NullCommandType::DrawIndexed).size();
    window.Rect({0.0f, 0.0f, 50.0f, 20.0f}, Fill({255, 255, 255, 255}));
    pipeline.FlushWindow();
    // A native overlay drawing now lands on top of the window drawing.
    CHECK(device.CommandsOf(NullCommandType::DrawIndexed).size() > presented);
    device.ReleaseState();
    const std::size_t flushed = device.Commands().size();
    pipeline.EndFrame();
    // Nothing of the window drawing is left for after the overlay.
    for (std::size_t i = flushed; i < device.Commands().size(); ++i)
        CHECK(device.Commands()[i].type != NullCommandType::DrawIndexed);
}

TEST_CASE("Engine.RenderPipeline.PresentsTheCanvasLetterboxedIntoTheBackBuffer", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    Painter& paint = pipeline.BeginFrame({1000, 400}, 1.0, 0.016f);
    paint.Circle({200.0f, 100.0f}, 20.0f, Fill({255, 0, 0, 255}));
    (void)pipeline.FinishCanvas();
    (void)pipeline.BeginWindow();
    // Native tool overlays must see the completed presentation before drawing into the window.
    CHECK(device.CommandsOf(NullCommandType::DrawIndexed).size() == 2);
    pipeline.EndFrame();
    const auto passes = device.CommandsOf(NullCommandType::BeginPass);
    REQUIRE(passes.size() == 2); // Canvas, window (no post-processing configured)
    CHECK(passes[0]->pass.clear->a == Approx(1.0f));
    CHECK_FALSE(passes[1]->pass.target.Valid());
    const auto draws = device.CommandsOf(NullCommandType::DrawIndexed);
    REQUIRE(draws.size() == 2);
    const auto image = device.DrawnVertices(*draws[1]);
    float left = 1e9f;
    float right = -1e9f;
    for (const PaintVertex& vertex : image) {
        CHECK(vertex.kind == PaintKind::Texture);
        left = std::min(left, vertex.x);
        right = std::max(right, vertex.x);
    }
    CHECK(left == Approx(100.0f));
    CHECK(right == Approx(900.0f));
    CHECK(device.BackBuffer() == PixelRect{0, 0, 1000, 400});
    CHECK(pipeline.LastFrame().drawCalls == 2);
    CHECK(pipeline.LastFrame().passes == 2);
}

TEST_CASE("Engine.RenderPipeline.BloomRunsFromTheEmissionLayer", "[engine][paint][render]") {
    NullDevice device;
    PostSettings post;
    post.bloom.intensity = 0.8f;
    RenderPipeline pipeline(device, Settings(post));
    Frame(pipeline, {800, 400});
    const auto passes = device.CommandsOf(NullCommandType::BeginPass);
    CHECK(device.Describe(passes.front()->pass.target).emission);
    const auto pipelines = device.CommandsOf(NullCommandType::SetPipeline);
    std::size_t down = 0;
    std::size_t up = 0;
    std::size_t compose = 0;
    for (const NullCommand* command : pipelines) {
        down += command->program == GpuProgram::BloomDownsample ? 1 : 0;
        up += command->program == GpuProgram::BloomUpsample ? 1 : 0;
        compose += command->program == GpuProgram::Compose ? 1 : 0;
    }
    CHECK(down >= 3);
    CHECK(up == down - 1);
    CHECK(compose == 1);

    RenderQuality off;
    off.bloom = BloomQuality::Off;
    pipeline.SetQuality(off);
    device.ClearCommands();
    Frame(pipeline, {800, 400});
    CHECK(device.CommandsOf(NullCommandType::DrawFullscreen).empty()); // Nothing else asks for post-processing
    CHECK_FALSE(device.Describe(device.CommandsOf(NullCommandType::BeginPass).front()->pass.target).emission);
}

TEST_CASE("Engine.RenderPipeline.FramePostChangesLastOneFrame", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    Painter& paint = pipeline.BeginFrame({800, 400}, 0.0, 0.0f);
    paint.Post().vignette = 0.5f;
    (void)pipeline.FinishCanvas();
    (void)pipeline.BeginWindow();
    pipeline.EndFrame();
    const auto composed = device.CommandsOf(NullCommandType::DrawFullscreen);
    REQUIRE(composed.size() == 1);
    CHECK(composed[0]->post.vignette == Approx(0.5f));
    device.ClearCommands();
    Frame(pipeline, {800, 400});
    CHECK(device.CommandsOf(NullCommandType::DrawFullscreen).empty());
}

TEST_CASE("Engine.RenderPipeline.FrameRequestedBloomStartsOnTheNextFrame", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings()); // Defaults: no bloom, no bloom layer
    const auto bloomFrame = [&] {
        device.ClearCommands();
        Painter& paint = pipeline.BeginFrame({800, 400}, 0.0, 0.0f);
        paint.Post().bloom.intensity = 1.0f;
        (void)pipeline.FinishCanvas();
        (void)pipeline.BeginWindow();
        pipeline.EndFrame();
        std::size_t passes = 0;
        for (const NullCommand* command : device.CommandsOf(NullCommandType::SetPipeline))
            passes += command->program == GpuProgram::BloomDownsample ? 1 : 0;
        return passes;
    };
    CHECK(bloomFrame() == 0); // The layer is reserved for the next frame
    CHECK(bloomFrame() > 0);
}
