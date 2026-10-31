#include "Graphics/Render/Internals/Renderer.h"
#include <raylib.h>
#include <rlgl.h>
#include "Graphics/Gpu/Internals/GlDevice.h"
#include "Graphics/Paint/Painter.h"

namespace {
// Native code (frame capture, rlImGui) draws through rlgl next. The device has restored GL's default bindings
// (ReleaseState); rlgl also caches its blend mode and skips setting the one it believes is current: switching away
// and back makes it apply alpha blending.
void HandOverToRlgl(GpuDevice& device) {
    device.ReleaseState();
    rlSetBlendMode(RL_BLEND_ADDITIVE);
    rlSetBlendMode(RL_BLEND_ALPHA);
}
} // namespace

Renderer::Frame::Frame(Renderer& renderer, Painter& canvas)
    : _renderer(renderer)
    , _canvas(canvas) {}

Renderer::Frame::~Frame() {
    if (_completed) return;
    try {
        Complete();
    } catch (...) {
        // Already unwinding (or shutting down); the next frame starts clean.
    }
}

void Renderer::Frame::Complete(const std::function<void(const Texture2D&)>& canvasReady,
                               const std::function<void(Painter&)>& windowDraw,
                               const std::function<void()>& toolOverlay) {
    if (_completed) return;
    _completed = true;
    Renderer& renderer = _renderer;
    FrameObserver* observer = renderer._observer;
    RenderPipeline& pipeline = *renderer._pipeline;
    const GpuTarget image = pipeline.FinishCanvas();
    if (observer) {
        observer->EndRegion(FrameRegion::Canvas);
        observer->BeginRegion(FrameRegion::PreviewBridge);
    }
    if (canvasReady) {
        HandOverToRlgl(*renderer._device);
        const auto native = renderer._device->Native(image);
        canvasReady({native.colorTexture, native.width, native.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
        rlDrawRenderBatchActive();
    }
    if (observer) {
        observer->EndRegion(FrameRegion::PreviewBridge);
        observer->BeginRegion(FrameRegion::Present);
    }
    Painter& window = pipeline.BeginWindow();
    if (windowDraw) windowDraw(window);
    pipeline.FlushWindow(); // Window drawing lies under the tool overlay
    if (toolOverlay) {
        HandOverToRlgl(*renderer._device);
        toolOverlay();
        rlDrawRenderBatchActive();
    }
    pipeline.EndFrame();
    if (observer) {
        observer->EndRegion(FrameRegion::Present);
        observer->BeginRegion(FrameRegion::EndDrawing);
    }
    EndDrawing(); // raylib: swap buffers, poll input, frame pacing
    if (observer) observer->EndRegion(FrameRegion::EndDrawing);
}

Renderer::Renderer(const WindowSettings& window, const RenderPipelineSettings& settings)
    : _window(window)
    , _device(std::make_unique<GlDevice>())
    , _pipeline(std::make_unique<RenderPipeline>(*_device, settings)) {}

Renderer::~Renderer() {
    _pipeline.reset();
    _device.reset();
}

bool Renderer::ShouldClose() const { return _window.ShouldClose(); }

Renderer::Frame Renderer::BeginFrame(double time, float delta) {
    if (_observer) _observer->BeginRegion(FrameRegion::Canvas);
    BeginDrawing(); // raylib: frame timing
    Painter& canvas = _pipeline->BeginFrame({_window.PixelWidth(), _window.PixelHeight()}, time, delta);
    return Frame(*this, canvas);
}

std::optional<Vec2> Renderer::ToCanvas(Vec2 windowPoint) const {
    return _pipeline->ToCanvas(windowPoint, {_window.Width(), _window.Height()});
}
