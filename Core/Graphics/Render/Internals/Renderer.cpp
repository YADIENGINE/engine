#include "Graphics/Render/Internals/Renderer.h"
#include <raylib.h>
#include "Graphics/Gpu/Internals/GlDevice.h"
#include "Graphics/Paint/Painter.hpp"

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
                               const std::function<void(Painter&)>& windowDraw) {
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
        const auto native = renderer._device->Native(image);
        canvasReady({native.colorTexture, native.width, native.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8});
        renderer._device->InvalidateState(); // The capture may read through other GL state
    }
    if (observer) {
        observer->EndRegion(FrameRegion::PreviewBridge);
        observer->BeginRegion(FrameRegion::Present);
    }
    Painter& window = pipeline.BeginWindow();
    if (windowDraw) windowDraw(window);
    renderer._device->InvalidateState(); // Tool overlays may render directly through their own GL backend.
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
