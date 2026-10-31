#include "Graphics/Render/Internals/RenderPipeline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "Graphics/Surface/Internals/TextureRefAccess.h"

namespace {
PremultipliedColor Premultiply(Color4 color) {
    const float alpha = static_cast<float>(color.a) / 255.0f;
    return {static_cast<float>(color.r) / 255.0f * alpha, static_cast<float>(color.g) / 255.0f * alpha,
            static_cast<float>(color.b) / 255.0f * alpha, alpha};
}
} // namespace

RenderPipeline::RenderPipeline(GpuDevice& device, const RenderPipelineSettings& settings)
    : _device(device)
    , _settings(settings)
    , _resources(device)
    , _glyphs(device)
    , _executor(device)
    , _context(_list, _resources, _executor, &_text)
    , _post(device) {
    if (!(settings.canvasSize.x > 0.0f) || !(settings.canvasSize.y > 0.0f))
        throw std::invalid_argument("RenderPipeline: the canvas needs a positive size");
}

RenderPipeline::~RenderPipeline() {
    if (_canvas.Valid()) _device.DestroyTarget(_canvas);
}

FloatRect RenderPipeline::Letterbox(Vec2 windowSize) const {
    const Float2 canvas = _settings.canvasSize;
    const auto windowWidth = static_cast<float>(windowSize.x);
    const auto windowHeight = static_cast<float>(windowSize.y);
    const float scale = std::min(windowWidth / canvas.x, windowHeight / canvas.y);
    // Whole pixels: an upscaled canvas texture lands on the window 1:1.
    const float width = std::round(canvas.x * scale);
    const float height = std::round(canvas.y * scale);
    return {std::floor((windowWidth - width) / 2.0f), std::floor((windowHeight - height) / 2.0f), width, height};
}

std::optional<Vec2> RenderPipeline::ToCanvas(Vec2 windowPoint, Vec2 windowSize) const {
    if (windowSize.x <= 0 || windowSize.y <= 0) return std::nullopt;
    const FloatRect dest = Letterbox(windowSize);
    const Float2 point{static_cast<float>(windowPoint.x), static_cast<float>(windowPoint.y)};
    if (!dest.Contains(point)) return std::nullopt;
    return Vec2{static_cast<int>((point.x - dest.x) * _settings.canvasSize.x / dest.width),
                static_cast<int>((point.y - dest.y) * _settings.canvasSize.y / dest.height)};
}

void RenderPipeline::PrepareCanvas(Vec2 windowSize, bool emission) {
    const Float2 logical = _settings.canvasSize;
    const auto logicalWidth = static_cast<int>(std::lround(logical.x));
    const auto logicalHeight = static_cast<int>(std::lround(logical.y));
    int width = logicalWidth;
    int height = logicalHeight;
    if (windowSize.x > 0 && windowSize.y > 0) {
        const FloatRect dest = Letterbox(windowSize);
        // Upscaled: rasterize at the window's resolution. Downscaled: draw at the logical size and filter down, so
        // thin lines survive.
        if (dest.width > logical.x) {
            width = static_cast<int>(dest.width);
            height = static_cast<int>(dest.height);
        }
    }
    if (_maxRenderHeight > 0 && height > std::max(_maxRenderHeight, logicalHeight)) {
        height = std::max(_maxRenderHeight, logicalHeight);
        width = static_cast<int>(std::lround(static_cast<float>(height) * logical.x / logical.y));
    }
    if (_canvas.Valid() && width == _canvasWidth && height == _canvasHeight && emission == _canvasEmission) return;
    if (_canvas.Valid()) _device.DestroyTarget(_canvas);
    _canvas =
        _device.CreateTarget({width, height, GpuFormat::Rgba8,
                              _settings.smoothCanvas ? GpuFilter::Linear : GpuFilter::Nearest, emission, "Canvas"});
    _canvasWidth = width;
    _canvasHeight = height;
    _canvasEmission = emission;
}

void RenderPipeline::BeginRecording(const PaintTarget& root, std::optional<PremultipliedColor> clear) {
    _list.Reset(root, clear);
    _context.scopes.clear();
}

void RenderPipeline::FinishRecording() {
    _list.Finish();
    FlushPaint(_context);
    _stats.vertices += static_cast<std::uint32_t>(_list.Vertices().size());
    _stats.indices += static_cast<std::uint32_t>(_list.Indices().size());
    _stats.paintErrors += _list.Rejected();
    if (_list.Rejected() > 0 && _list.FirstRejection() != _reportedError) {
        _reportedError = _list.FirstRejection();
        std::fprintf(stderr, "Painter: %s (%u skipped this frame)\n", _reportedError.c_str(), _list.Rejected());
    }
}

Painter& RenderPipeline::BeginFrame(Vec2 windowSize, double time, float delta) {
    if (_phase != Phase::Idle) throw std::logic_error("RenderPipeline: frame already begun");
    _windowSize = windowSize;
    _stats = {};
    _context.frame = {time, delta, _quality, _settings.post};
    // The canvas keeps a bloom layer while the defaults or the last frame asked for bloom (a frame that turns bloom
    // on through Painter::Post blooms from the next frame on).
    PrepareCanvas(windowSize, PostChain::Blooms(_settings.post, _quality) ||
                                  (_bloomRequested && _quality.bloom != BloomQuality::Off));
    _device.BeginFrame();
    _device.SetBackBufferSize(windowSize.x, windowSize.y);
    const Float2 scale{static_cast<float>(_canvasWidth) / _settings.canvasSize.x,
                       static_cast<float>(_canvasHeight) / _settings.canvasSize.y};
    _resources.BeginFrame(scale);
    _glyphs.BeginFrame();
    _executor.ResetCounters();
    BeginRecording({_canvas, _settings.canvasSize, {0, 0, _canvasWidth, _canvasHeight}, _canvasEmission},
                   Premultiply(_settings.background));
    _phase = Phase::Canvas;
    return _painter;
}

GpuTarget RenderPipeline::FinishCanvas() {
    if (_phase != Phase::Canvas) throw std::logic_error("RenderPipeline: no canvas to finish");
    FinishRecording();
    PostSettings post = _context.frame.post;
    _bloomRequested = PostChain::Blooms(post, _quality);
    if (!_canvasEmission) post.bloom.intensity = 0.0f;
    _presented = PostChain::Active(post, _quality)
                     ? _post.Run(_canvas, _canvasWidth, _canvasHeight, _settings.canvasSize.y, post, _quality,
                                 static_cast<float>(_context.frame.time))
                     : _canvas;
    _phase = Phase::Window;
    // Window phase: the whole back buffer in device pixels, the canvas image letterboxed into it.
    const Float2 window{static_cast<float>(std::max(_windowSize.x, 1)), static_cast<float>(std::max(_windowSize.y, 1))};
    BeginRecording({{}, window, {0, 0, std::max(_windowSize.x, 1), std::max(_windowSize.y, 1)}, false},
                   Premultiply(_settings.letterbox));
    const FloatRect dest = Letterbox(_windowSize);
    const TextureRef image = TextureRefAccess::Make(_device.TargetTexture(_presented, 0), _canvasWidth, _canvasHeight,
                                                    _settings.canvasSize.x, _settings.canvasSize.y, true);
    _painter.Image(image, dest);
    return _presented;
}

Painter& RenderPipeline::BeginWindow() {
    if (_phase != Phase::Window) throw std::logic_error("RenderPipeline: finish the canvas first");
    FlushPaint(_context); // The canvas lands in the window first; window drawing goes on top of it.
    return _painter;
}

void RenderPipeline::FlushWindow() {
    if (_phase != Phase::Window) throw std::logic_error("RenderPipeline: finish the canvas first");
    FlushPaint(_context);
}

void RenderPipeline::EndFrame() {
    if (_phase != Phase::Window) throw std::logic_error("RenderPipeline: finish the canvas first");
    _phase = Phase::Idle;
    FinishRecording();
    const GpuFrameCounters& counters = _device.Counters();
    _stats.drawCalls = counters.drawCalls;
    _stats.passes = counters.passes;
    _stats.uploadedBytes = counters.uploadedBytes;
    _stats.pipelineChanges = counters.pipelineChanges;
    _stats.scissorChanges = counters.scissorChanges;
    _stats.textureBinds = counters.textureBinds;
    _stats.batches = _executor.BatchesExecuted();
    _stats.glyphsRasterized = _glyphs.RasterizedThisFrame();
    _stats.layoutHits = _layouts.HitsThisFrame();
    _stats.layoutMisses = _layouts.MissesThisFrame();
    _stats.surfacesAllocated = _resources.SurfacesAllocatedThisFrame();
    _device.EndFrame();
    _resources.EndFrame();
    _layouts.EndFrame();
}
