#include "Graphics/Render/Internals/RenderResources.h"
#include <cmath>
#include "Graphics/Surface/Surface.h"

namespace {
constexpr int PoolIdleFrames = 120; // A pooled image unused this long is freed
} // namespace

RenderResources::RenderResources(GpuDevice& device)
    : _device(device)
    , _retirement(std::make_shared<GpuRetirement>()) {
    _retirement->device = &device;
}

RenderResources::~RenderResources() {
    DestroyRetired();
    for (const Pooled& image : _pool)
        _device.DestroyTarget(image.target);
    _retirement->device = nullptr; // Surfaces that outlive us release nothing
}

void RenderResources::BeginFrame(Float2 canvasPixelScale) {
    _scale = canvasPixelScale;
    _allocations = 0;
}

void RenderResources::EndFrame() {
    DestroyRetired();
    for (std::size_t i = _pool.size(); i-- > 0;) {
        Pooled& image = _pool[i];
        image.idleFrames = image.used ? 0 : image.idleFrames + 1;
        image.used = false;
        image.inUse = false;
        if (image.idleFrames > PoolIdleFrames) {
            _device.DestroyTarget(image.target);
            _pool.erase(_pool.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
}

void RenderResources::DestroyRetired() {
    for (const GpuTarget target : _retirement->targets)
        _device.DestroyTarget(target);
    for (const GpuTexture texture : _retirement->textures)
        _device.DestroyTexture(texture);
    _retirement->targets.clear();
    _retirement->textures.clear();
}

Float2 RenderResources::PixelSize(const Surface& surface, Float2 resolution) const {
    const SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    if (state.options.pixels) return state.size; // Whole numbers already (Surface)
    const Float2 scale = resolution.x > 0.0f && resolution.y > 0.0f ? resolution : _scale;
    return {std::max(1.0f, std::ceil(state.size.x * scale.x - 1e-3f)),
            std::max(1.0f, std::ceil(state.size.y * scale.y - 1e-3f))};
}

PaintTarget RenderResources::Prepare(Surface& surface, Float2 resolution) {
    SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    const Float2 pixels = PixelSize(surface, resolution);
    const int width = static_cast<int>(pixels.x);
    const int height = static_cast<int>(pixels.y);
    if (!state.target.Valid() || state.pixelWidth != width || state.pixelHeight != height ||
        state.retirement != _retirement) {
        state.Release();
        state.retirement = _retirement;
        const bool smooth = state.options.smooth && !state.options.pixels;
        state.target =
            _device.CreateTarget({width, height, GpuFormat::Rgba8, smooth ? GpuFilter::Linear : GpuFilter::Nearest,
                                  state.options.emission, state.options.pixels ? "PixelSurface" : "Surface"});
        state.color = _device.TargetTexture(state.target, 0);
        state.emission = state.options.emission ? _device.TargetTexture(state.target, 1) : GpuTexture{};
        state.pixelWidth = width;
        state.pixelHeight = height;
        ++_allocations;
    }
    state.allocatedScale = {pixels.x / state.size.x, pixels.y / state.size.y};
    PaintTarget target{state.target, state.size, {0, 0, width, height}, state.options.emission};
    // A pixel surface is its own resolution, and draws hard edges; any other passes on the one it was drawn from.
    target.hard = state.options.pixels;
    target.resolution = state.options.pixels ? target.PixelScale() : resolution;
    return target;
}

void RenderResources::MarkDrawn(Surface& surface) { SurfaceAccess::Of(surface).drawn = true; }

bool RenderResources::Stale(const Surface& surface, Float2 resolution) const {
    const SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    if (!state.drawn || !state.target.Valid() || state.retirement != _retirement) return true;
    const Float2 pixels = PixelSize(surface, resolution);
    return state.pixelWidth != static_cast<int>(pixels.x) || state.pixelHeight != static_cast<int>(pixels.y);
}

PaintTarget RenderResources::AcquireTransient(const PaintTarget& parent) {
    const PixelRect& viewport = parent.viewport;
    const bool emission = parent.emission;
    PaintTarget image = parent;
    image.viewport = {0, 0, viewport.width, viewport.height};
    for (Pooled& pooled : _pool) {
        if (!pooled.inUse && pooled.width == viewport.width && pooled.height == viewport.height &&
            pooled.emission == emission) {
            pooled.inUse = true;
            pooled.used = true;
            image.target = pooled.target;
            return image;
        }
    }
    image.target =
        _device.CreateTarget({viewport.width, viewport.height, GpuFormat::Rgba8, GpuFilter::Linear, emission, "Group"});
    _pool.push_back({image.target, viewport.width, viewport.height, emission, true, true, 0});
    ++_allocations;
    return image;
}

void RenderResources::ReleaseTransient(GpuTarget target) {
    for (Pooled& image : _pool)
        if (image.target == target) image.inUse = false;
}

GpuTexture RenderResources::Attachment(GpuTarget target, int attachment) const {
    return _device.TargetTexture(target, attachment);
}
