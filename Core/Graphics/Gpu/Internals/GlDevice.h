#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Gpu/Internals/HandlePool.h"

// GpuDevice over OpenGL 3.3 core. Needs a current context (the window's) for its whole life; owns every GL object it
// creates and tracks GL state; native code (frame capture, tool overlays) runs only after ReleaseState. GL functions
// come from the loader raylib already initialised.
class GlDevice final : public GpuDevice {
public:
    GlDevice();
    ~GlDevice() override;

    [[nodiscard]] GpuTexture CreateTexture(const GpuTextureDesc& desc) override;
    void UpdateTexture(GpuTexture texture, PixelRect region, std::span<const std::byte> pixels) override;
    void DestroyTexture(GpuTexture texture) override;
    [[nodiscard]] GpuTarget CreateTarget(const GpuTargetDesc& desc) override;
    [[nodiscard]] GpuTexture TargetTexture(GpuTarget target, int attachment) const override;
    void DestroyTarget(GpuTarget target) override;
    [[nodiscard]] GpuNativeTarget Native(GpuTarget target) const override;

    void BeginFrame() override;
    void SetBackBufferSize(int width, int height) override;
    void EndFrame() override;
    [[nodiscard]] GpuGeometry Upload(std::span<const PaintVertex> vertices,
                                     std::span<const std::uint32_t> indices) override;
    void BeginPass(const GpuPassDesc& pass) override;
    void EndPass() override;
    void SetPipeline(GpuProgram program, GpuBlend blend) override;
    void SetScissor(const PixelRect* rect) override;
    void BindTextures(std::span<const GpuTexture> textures) override;
    void DrawIndexed(const GpuGeometry& geometry, std::uint32_t firstIndex, std::uint32_t indexCount) override;
    void DrawFullscreen(const PostUniforms& uniforms) override;
    void ReleaseState() override;
    void PushMarker(std::string_view name) override;
    void PopMarker() override;
    [[nodiscard]] const GpuFrameCounters& Counters() const override { return _counters; }

private:
    struct Texture {
        unsigned id = 0;
        GpuTextureDesc desc;
        bool owned = true; // False for a target's attachments
    };
    struct Target {
        unsigned framebuffer = 0;
        GpuTexture color;
        GpuTexture emission;
        GpuTargetDesc desc;
    };
    struct Program {
        unsigned id = 0;
        int viewSize = -1;
        int sourceTexel = -1;
        int bloomIntensity = -1;
        int bloomSpread = -1;
        int vignette = -1;
        int scanlines = -1;
        int scanlineCount = -1;
        int aberration = -1;
    };
    struct Stream {
        unsigned buffer = 0;
        std::size_t capacity = 0;
        std::size_t offset = 0;
    };
    [[nodiscard]] GpuTexture AddTexture(const GpuTextureDesc& desc, bool owned);
    [[nodiscard]] std::size_t Write(Stream& stream, const void* data, std::size_t bytes);
    [[nodiscard]] int TargetHeight() const;
    void ApplyBlend(GpuBlend blend);
    void ForgetState();

    HandlePool<GpuTextureTag, Texture> _textures;
    HandlePool<GpuTargetTag, Target> _targets;
    std::array<Program, 4> _programs{}; // GpuProgram order
    unsigned _paintLayout = 0;          // Vertex array of the paint pipeline
    unsigned _emptyLayout = 0;          // Vertex array for full-screen triangles
    Stream _vertices;
    Stream _indices;
    unsigned _white = 0; // 1x1 texture in unused slots
    GpuFrameCounters _counters;
    int _backBufferWidth = 1;
    int _backBufferHeight = 1;
    bool _debugGroups = false;

    // Current pass and cached GL state (nullopt: unknown, set on next use).
    GpuPassDesc _pass;
    bool _passEmission = false;
    std::optional<GpuProgram> _program;
    std::optional<GpuBlend> _blend;
    std::optional<std::optional<PixelRect>> _scissor;
    std::array<unsigned, PaintTextureSlots> _bound{};
    bool _boundKnown = false;
};
