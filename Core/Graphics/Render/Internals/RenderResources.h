#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/TextureRef.h"

class Surface;

// GPU memory behind Painter: surfaces (allocated at the canvas resolution), the pool of temporary group images and
// deferred destruction. Owned by the renderer; lives as long as its device.
class RenderResources {
public:
    explicit RenderResources(GpuDevice& device);
    ~RenderResources();

    [[nodiscard]] GpuDevice& Device() const { return _device; }
    // Device pixels per logical unit of the canvas this frame; surfaces follow it.
    void BeginFrame(Float2 canvasPixelScale);
    // After the frame executed: destroys released objects, frees the group pool's unused images.
    void EndFrame();
    [[nodiscard]] Float2 CanvasPixelScale() const { return _scale; }

    // The surface's target at the current resolution, (re)allocating it when needed (the image is then lost).
    [[nodiscard]] PaintTarget Prepare(Surface& surface);
    void MarkDrawn(Surface& surface);
    [[nodiscard]] bool Stale(const Surface& surface) const;

    // A cleared-before-use image of the given size for a group; returned to the pool at EndFrame. Within a frame
    // an image is reused only after the group using it was composited (execution is in recording order).
    [[nodiscard]] PaintTarget AcquireTransient(Float2 size, PixelRect viewport, bool emission);
    void ReleaseTransient(GpuTarget target);
    [[nodiscard]] GpuTexture Attachment(GpuTarget target, int attachment) const;

    [[nodiscard]] std::uint32_t SurfacesAllocatedThisFrame() const { return _allocations; }
    [[nodiscard]] std::size_t PooledImages() const { return _pool.size(); }

private:
    struct Pooled {
        GpuTarget target;
        int width = 0;
        int height = 0;
        bool emission = false;
        bool inUse = false;
        bool used = false; // Acquired since the last EndFrame
        int idleFrames = 0;
    };
    [[nodiscard]] Float2 PixelSize(Float2 size) const;
    void DestroyRetired();

    GpuDevice& _device;
    std::shared_ptr<GpuRetirement> _retirement;
    std::vector<Pooled> _pool;
    Float2 _scale{1.0f, 1.0f};
    std::uint32_t _allocations = 0;

public:
    RenderResources(const RenderResources&) = delete;
    RenderResources& operator=(const RenderResources&) = delete;
};
