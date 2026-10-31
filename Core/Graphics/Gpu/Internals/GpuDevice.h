#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include "Graphics/Gpu/Internals/GpuTypes.h"
#include "Graphics/Gpu/Internals/PaintVertex.h"

// Counters a device keeps per frame (reset by BeginFrame).
struct GpuFrameCounters {
    std::uint32_t drawCalls = 0;
    std::uint32_t passes = 0;
    std::uint32_t pipelineChanges = 0;
    std::uint32_t scissorChanges = 0;
    std::uint32_t textureBinds = 0;
    std::uint64_t uploadedBytes = 0;
};

// The engine's only door to the GPU. Implementations: GlDevice (OpenGL 3.3 core) and NullDevice (tests). It knows
// the paint vertex format and the built-in programs; everything above it (Painter, DrawList, FrameExecutor) is
// GPU-independent. Not thread-safe: the game thread owns it.
class GpuDevice {
public:
    virtual ~GpuDevice() = default;

    [[nodiscard]] virtual GpuTexture CreateTexture(const GpuTextureDesc& desc) = 0;
    // Rows top to bottom, tightly packed in the texture's format (Rgba8: premultiplied).
    virtual void UpdateTexture(GpuTexture texture, PixelRect region, std::span<const std::byte> pixels) = 0;
    virtual void DestroyTexture(GpuTexture texture) = 0;
    [[nodiscard]] virtual GpuTarget CreateTarget(const GpuTargetDesc& desc) = 0;
    // Attachment 0: colour, 1: emission (when created with one). The texture lives as long as the target.
    [[nodiscard]] virtual GpuTexture TargetTexture(GpuTarget target, int attachment) const = 0;
    virtual void DestroyTarget(GpuTarget target) = 0;
    [[nodiscard]] virtual GpuNativeTarget Native(GpuTarget target) const = 0;

    // A frame: BeginFrame, any number of uploads and passes, EndFrame.
    virtual void BeginFrame() = 0;
    // Pixel size of the window's back buffer (passes with a null target) for this frame.
    virtual void SetBackBufferSize(int width, int height) = 0;
    virtual void EndFrame() = 0;
    // Copies geometry into the streaming buffers; valid for draws until EndFrame. Indices count from the first
    // uploaded vertex.
    [[nodiscard]] virtual GpuGeometry Upload(std::span<const PaintVertex> vertices,
                                             std::span<const std::uint32_t> indices) = 0;
    virtual void BeginPass(const GpuPassDesc& pass) = 0;
    virtual void EndPass() = 0;
    virtual void SetPipeline(GpuProgram program, GpuBlend blend) = 0;
    // Device pixels of the current pass's target; null disables clipping.
    virtual void SetScissor(const PixelRect* rect) = 0;
    // Binds textures to slots 0..n-1 for the following draws.
    virtual void BindTextures(std::span<const GpuTexture> textures) = 0;
    virtual void DrawIndexed(const GpuGeometry& geometry, std::uint32_t firstIndex, std::uint32_t indexCount) = 0;
    // Post programs: a full-viewport triangle sampling slot 0 (and 1).
    virtual void DrawFullscreen(const PostUniforms& uniforms) = 0;

    // Hands the context to native code (frame capture, tool overlays): default bindings (back buffer over the whole
    // window, no program, vertex array or scissor, texture unit 0) and nothing cached, so whatever the native code
    // changes is set again on the device's next use. Only between passes.
    virtual void ReleaseState() = 0;
    virtual void PushMarker(std::string_view name) = 0;
    virtual void PopMarker() = 0;
    [[nodiscard]] virtual const GpuFrameCounters& Counters() const = 0;

protected:
    GpuDevice() = default;

public:
    GpuDevice(const GpuDevice&) = delete;
    GpuDevice& operator=(const GpuDevice&) = delete;
    GpuDevice(GpuDevice&&) = delete;
    GpuDevice& operator=(GpuDevice&&) = delete;
};
