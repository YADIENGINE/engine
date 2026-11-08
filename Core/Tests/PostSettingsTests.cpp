#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "Animation/Tween.h"
#include "Graphics/Gpu/Internals/NullDevice.h"
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Render/Internals/PostChain.h"
#include "Graphics/Render/Internals/RenderPipeline.h"

using Catch::Approx;

namespace {
RenderPipelineSettings Settings() {
    RenderPipelineSettings settings;
    settings.canvasSize = {400.0f, 200.0f};
    return settings;
}

const NullCommand* ComposeFrame(NullDevice& device, RenderPipeline& pipeline, const PostSettings& post, double time) {
    device.ClearCommands();
    Painter& paint = pipeline.BeginFrame({800, 400}, time, 0.016f);
    paint.Post() = post;
    (void)pipeline.FinishCanvas();
    (void)pipeline.BeginWindow();
    pipeline.EndFrame();
    const auto drawn = device.CommandsOf(NullCommandType::DrawFullscreen);
    return drawn.empty() ? nullptr : drawn.back();
}

PostSettings Haunted() {
    PostSettings post;
    post.vignette = 0.8f;
    post.fade = 0.6f;
    post.tint = {.color = Rgb(0x4060a0), .amount = 1.0f};
    post.grain = {.amount = 0.4f, .size = 2.0f};
    post.wobble = {.amount = 8.0f, .speed = 3.0f};
    post.tape = {.band = 1.0f, .shift = 2.0f, .speed = 2.0f};
    return post;
}
} // namespace

TEST_CASE("Engine.PostChain.ActiveKnowsEveryEffect", "[engine][paint][render]") {
    const RenderQuality quality;
    const auto active = [&](auto change) {
        PostSettings post;
        change(post);
        return PostChain::Active(post, quality);
    };
    CHECK_FALSE(PostChain::Active({}, quality));
    CHECK(active([](PostSettings& post) { post.bloom.intensity = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.vignette = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.scanlines = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.aberration = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.fade = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.tint = {.color = Rgb(0x304060), .amount = 0.5f}; }));
    CHECK(active([](PostSettings& post) { post.grain.amount = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.wobble.amount = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.tape.band = 0.5f; }));
    CHECK(active([](PostSettings& post) { post.tape.shift = 0.5f; }));
}

TEST_CASE("Engine.PostChain.SettingsThatChangeNothingAreNotActive", "[engine][paint][render]") {
    const RenderQuality quality;
    PostSettings post;
    post.tint.color = Rgb(0x304060); // A colour at amount 0
    post.grain.size = 3.0f;
    post.wobble.speed = 4.0f;
    post.tape.speed = 4.0f;
    CHECK_FALSE(PostChain::Active(post, quality));
    post.tint = {.color = Rgb(0xffffff), .amount = 1.0f}; // White multiplies to the same image
    CHECK_FALSE(PostChain::Active(post, quality));
}

TEST_CASE("Engine.PostChain.ScreenMotionOffStopsOnlyWhatMoves", "[engine][paint][render]") {
    const RenderQuality still{.screenMotion = false};
    PostSettings post;
    post.grain.amount = 0.5f;
    post.wobble.amount = 5.0f;
    post.tape = {.band = 1.0f, .shift = 3.0f};
    CHECK_FALSE(PostChain::Active(post, still)); // Nothing left to do: no compose pass at all
    post.fade = 0.5f;
    CHECK(PostChain::Active(post, still));

    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    pipeline.SetQuality(still);
    const NullCommand* composed = ComposeFrame(device, pipeline, post, 1.0);
    REQUIRE(composed != nullptr);
    CHECK(composed->post.fade == Approx(0.5f));
    CHECK(composed->post.grain == 0.0f);
    CHECK(composed->post.wobble == 0.0f);
    CHECK(composed->post.tapeBand == 0.0f);
    CHECK(composed->post.tapeShift == 0.0f);
}

TEST_CASE("Engine.PostChain.NothingAskedDrawsNoComposePass", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    CHECK(ComposeFrame(device, pipeline, {}, 1.0) == nullptr);
    CHECK(ComposeFrame(device, pipeline, Mix(PostSettings{}, Haunted(), 0.0f), 2.0) == nullptr);
}

TEST_CASE("Engine.PostChain.CanvasUnitsBecomeTexels", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings()); // Canvas 200 tall in a window 400 tall: two texels per unit
    const NullCommand* composed = ComposeFrame(device, pipeline, Haunted(), 1.0);
    REQUIRE(composed != nullptr);
    CHECK(composed->post.wobble == Approx(16.0f));
    CHECK(composed->post.tapeShift == Approx(4.0f));
    CHECK(composed->post.grainCell == Approx(4.0f));
    CHECK(composed->post.tintRed == Approx(0x40 / 255.0f));
    CHECK(composed->post.tintBlue == Approx(0xa0 / 255.0f));
}

TEST_CASE("Engine.PostChain.ScreenRegionUsesLocalCoordinates", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    PostSettings post = Haunted();
    // Intentionally off-centre: canvas y is top-down, texture y is bottom-up.
    post.region = {40.0f, 20.0f, 320.0f, 150.0f};
    const NullCommand* composed = ComposeFrame(device, pipeline, post, 1.0);
    REQUIRE(composed != nullptr);
    CHECK(composed->post.region.x == Approx(0.1f));
    CHECK(composed->post.region.y == Approx(0.15f));
    CHECK(composed->post.region.width == Approx(0.8f));
    CHECK(composed->post.region.height == Approx(0.75f));
    CHECK(composed->post.scanlineCount == Approx(75.0f));
    CHECK(composed->post.wobble == Approx(16.0f)); // Distances are still canvas units.

    post.region = {};
    const auto* full = ComposeFrame(device, pipeline, post, 2.0);
    REQUIRE(full != nullptr);
    CHECK(full->post.region == FloatRect{0.0f, 0.0f, 1.0f, 1.0f});
    CHECK(full->post.scanlineCount == Approx(100.0f));
}

TEST_CASE("Engine.PostChain.OutsideScreenRegionKeepsUniformsFinite", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    PostSettings post = Haunted();
    // No overlap on either axis: every fragment must pass through before the shader's local-UV division.
    post.region = {500.0f, 300.0f, 80.0f, 40.0f};
    const auto* composed = ComposeFrame(device, pipeline, post, 1.0);
    REQUIRE(composed != nullptr);
    const auto& uniforms = composed->post;
    CHECK(uniforms.region.Empty());
    CHECK(std::isfinite(uniforms.region.x));
    CHECK(std::isfinite(uniforms.region.y));
    CHECK(std::isfinite(uniforms.region.width));
    CHECK(std::isfinite(uniforms.region.height));
    CHECK(std::isfinite(uniforms.scanlineCount));
    CHECK(uniforms.sourceTexel.x > 0.0f);
    CHECK(uniforms.sourceTexel.y > 0.0f);
}

TEST_CASE("Engine.PostSettings.MixPreservesTheSelectedScreen", "[engine][paint]") {
    PostSettings screen = Haunted();
    screen.region = {40.0f, 20.0f, 320.0f, 160.0f};
    CHECK(Mix(screen, screen, 0.5f).region == screen.region);
    CHECK(Mix({}, screen, 0.0f).region.Empty());
    CHECK(Mix({}, screen, 1.0f).region == screen.region);
}

TEST_CASE("Engine.PostChain.ValuesOutOfRangeAreHeldBack", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    PostSettings post;
    post.fade = 5.0f;
    post.grain = {.amount = 5.0f, .size = 0.0f};
    post.tint = {.color = Rgb(0x000000), .amount = 5.0f};
    const NullCommand* composed = ComposeFrame(device, pipeline, post, 1.0);
    REQUIRE(composed != nullptr);
    CHECK(composed->post.fade == 1.0f);
    CHECK(composed->post.grain == 1.0f);
    CHECK(composed->post.grainCell == 1.0f); // At least one texel: the shader divides by it
    CHECK(composed->post.tintRed == 0.0f);
}

TEST_CASE("Engine.PostChain.ASpeedChangeDoesNotJumpTheRhythm", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    PostSettings post = Haunted();
    post.wobble.speed = 1.0f;
    (void)ComposeFrame(device, pipeline, post, 50.0);
    const float before = ComposeFrame(device, pipeline, post, 50.5)->post.wobblePhase;
    post.wobble.speed = 2.0f;
    const float after = ComposeFrame(device, pipeline, post, 51.0)->post.wobblePhase;
    CHECK(after - before == Approx(1.0f)); // Half a second at speed 2, not 51 seconds' worth of difference
}

TEST_CASE("Engine.PostChain.AStoppedOrRewoundClockHoldsTheImage", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    const PostSettings post = Haunted();
    const NullCommand first = *ComposeFrame(device, pipeline, post, 10.0);
    const NullCommand paused = *ComposeFrame(device, pipeline, post, 10.0);
    CHECK(paused.post.noiseFrame == first.post.noiseFrame);
    CHECK(paused.post.wobblePhase == first.post.wobblePhase);
    CHECK(paused.post.tapeRoll == first.post.tapeRoll);
    const NullCommand rewound = *ComposeFrame(device, pipeline, post, 4.0);
    CHECK(rewound.post.wobblePhase == first.post.wobblePhase);
    CHECK(rewound.post.tapeRoll == first.post.tapeRoll);
}

TEST_CASE("Engine.PostChain.NoiseMovesOnALongRun", "[engine][paint][render]") {
    NullDevice device;
    RenderPipeline pipeline(device, Settings());
    const PostSettings post = Haunted();
    const double days = 40.0 * 24.0 * 3600.0; // Past where a float clock would stop telling frames apart
    const NullCommand first = *ComposeFrame(device, pipeline, post, days);
    const NullCommand next = *ComposeFrame(device, pipeline, post, days + 1.0 / 24.0);
    CHECK(next.post.noiseFrame != first.post.noiseFrame);
    CHECK(next.post.noiseFrame == std::floor(next.post.noiseFrame));
}

TEST_CASE("Engine.PostSettings.MixReachesBothEnds", "[engine][paint]") {
    const PostSettings calm;
    const PostSettings haunted = Haunted();
    const PostSettings start = Mix(calm, haunted, 0.0f);
    CHECK(start.vignette == 0.0f);
    CHECK(start.grain.amount == 0.0f);
    CHECK(start.wobble.amount == 0.0f);
    const PostSettings end = Mix(calm, haunted, 1.0f);
    CHECK(end.vignette == haunted.vignette);
    CHECK(end.tint.amount == haunted.tint.amount);
    CHECK(end.tint.color == haunted.tint.color);
    CHECK(end.tape.band == haunted.tape.band);
    CHECK(end.tape.shift == haunted.tape.shift);
}

TEST_CASE("Engine.PostSettings.MixMovesStrengthsLinearlyAndHoldsItsRange", "[engine][paint]") {
    const PostSettings haunted = Haunted();
    const PostSettings half = Mix({}, haunted, 0.5f);
    CHECK(half.fade == Approx(0.3f));
    CHECK(half.grain.amount == Approx(0.2f));
    CHECK(half.wobble.amount == Approx(4.0f));
    CHECK(Mix({}, haunted, -1.0f).fade == 0.0f); // A curve that overshoots stays between the two
    CHECK(Mix({}, haunted, 2.0f).fade == Approx(haunted.fade));
}

TEST_CASE("Engine.PostSettings.MixIgnoresTheShapeOfAnEffectThatIsOff", "[engine][paint]") {
    const PostSettings haunted = Haunted();
    const PostSettings start = Mix({}, haunted, 0.01f);
    CHECK(start.wobble.speed == haunted.wobble.speed);
    CHECK(start.tape.speed == haunted.tape.speed);
    CHECK(start.grain.size == haunted.grain.size);
    CHECK(start.tint.color == haunted.tint.color);
    const PostSettings back = Mix(haunted, {}, 0.99f);
    CHECK(back.wobble.speed == haunted.wobble.speed);
    CHECK(back.tint.color == haunted.tint.color);
    PostSettings other = haunted;
    other.wobble.speed = 1.0f;
    CHECK(Mix(other, haunted, 0.5f).wobble.speed == Approx(2.0f)); // On at both ends: it blends
}

TEST_CASE("Engine.PostSettings.MixDoesNotBlendAnAutomaticScanlineCount", "[engine][paint]") {
    PostSettings automatic;
    automatic.scanlines = 0.5f;
    PostSettings counted = automatic;
    counted.scanlineCount = 300.0f;
    CHECK(Mix(automatic, counted, 0.25f).scanlineCount == 0.0f);
    CHECK(Mix(automatic, counted, 0.75f).scanlineCount == 300.0f);
    PostSettings off;
    CHECK(Mix(off, counted, 0.25f).scanlineCount == 300.0f);
}

TEST_CASE("Engine.PostSettings.ATweenAnimatesAWholeFinish", "[engine][paint]") {
    const PostSettings haunted = Haunted();
    SecondsTween<PostSettings> finish{PostSettings{}};
    finish.To(haunted, 2.0f);
    finish.Update(1.0f);
    const PostSettings& halfway = finish;
    CHECK(halfway.fade == Approx(0.3f));
    finish.Update(1.0f);
    const PostSettings& end = finish;
    CHECK(end.fade == Approx(haunted.fade));
}
