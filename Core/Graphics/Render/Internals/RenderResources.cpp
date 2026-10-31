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

Float2 RenderResources::PixelSize(Float2 size) const {
    return {std::max(1.0f, std::ceil(size.x * _scale.x - 1e-3f)), std::max(1.0f, std::ceil(size.y * _scale.y - 1e-3f))};
}

PaintTarget RenderResources::Prepare(Surface& surface) {
    SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    const Float2 pixels = PixelSize(state.size);
    const int width = static_cast<int>(pixels.x);
    const int height = static_cast<int>(pixels.y);
    if (!state.target.Valid() || state.pixelWidth != width || state.pixelHeight != height ||
        state.retirement != _retirement) {
        state.Release();
        state.retirement = _retirement;
        state.target = _device.CreateTarget({width, height, GpuFormat::Rgba8,
                                             state.options.smooth ? GpuFilter::Linear : GpuFilter::Nearest,
                                             state.options.emission, "Surface"});
        state.color = _device.TargetTexture(state.target, 0);
        state.emission = state.options.emission ? _device.TargetTexture(state.target, 1) : GpuTexture{};
        state.pixelWidth = width;
        state.pixelHeight = height;
        ++_allocations;
    }
    state.allocatedScale = _scale;
    return {state.target, state.size, {0, 0, width, height}, state.options.emission};
}

void RenderResources::MarkDrawn(Surface& surface) { SurfaceAccess::Of(surface).drawn = true; }

bool RenderResources::Stale(const Surface& surface) const {
    const SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    if (!state.drawn || !state.target.Valid() || state.retirement != _retirement) return true;
    const Float2 pixels = PixelSize(state.size);
    return state.pixelWidth != static_cast<int>(pixels.x) || state.pixelHeight != static_cast<int>(pixels.y);
}

PaintTarget RenderResources::AcquireTransient(Float2 size, PixelRect viewport, bool emission) {
    for (Pooled& image : _pool) {
        if (!image.inUse && image.width == viewport.width && image.height == viewport.height &&
            image.emission == emission) {
            image.inUse = true;
            image.used = true;
            return {image.target, size, {0, 0, viewport.width, viewport.height}, emission};
        }
    }
    const GpuTarget target =
        _device.CreateTarget({viewport.width, viewport.height, GpuFormat::Rgba8, GpuFilter::Linear, emission, "Group"});
    _pool.push_back({target, viewport.width, viewport.height, emission, true, true, 0});
    ++_allocations;
    return {target, size, {0, 0, viewport.width, viewport.height}, emission};
}

void RenderResources::ReleaseTransient(GpuTarget target) {
    for (Pooled& image : _pool)
        if (image.target == target) image.inUse = false;
}

GpuTexture RenderResources::Attachment(GpuTarget target, int attachment) const {
    return _device.TargetTexture(target, attachment);
}
