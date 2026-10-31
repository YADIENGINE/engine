#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"

enum class NullCommandType {
    Upload,
    BeginPass,
    EndPass,
    SetPipeline,
    SetScissor,
    BindTextures,
    DrawIndexed,
    DrawFullscreen,
    Release,
    PushMarker,
    PopMarker
};

// One recorded device call; only the fields of its type are meaningful.
struct NullCommand {
    NullCommandType type = NullCommandType::Upload;
    GpuPassDesc pass;
    GpuProgram program = GpuProgram::Paint;
    GpuBlend blend = GpuBlend::Premultiplied;
    std::optional<PixelRect> scissor;
    std::vector<GpuTexture> textures;
    GpuGeometry geometry;
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    PostUniforms post;
    std::string marker;
};

// GPU-free device for tests: validates handles like a real device (stale or foreign handles throw logic_error),
// keeps texture contents in memory and records every call in order.
class NullDevice final : public GpuDevice {
public:
    NullDevice() = default;

    [[nodiscard]] GpuTexture CreateTexture(const GpuTextureDesc& desc) override;
    void UpdateTexture(GpuTexture texture, PixelRect region, std::span<const std::byte> pixels) override;
    void DestroyTexture(GpuTexture texture) override;
    [[nodiscard]] GpuTarget CreateTarget(const GpuTargetDesc& desc) override;
    [[nodiscard]] GpuTexture TargetTexture(GpuTarget target, int attachment) const override;
    void DestroyTarget(GpuTarget target) override;
    [[nodiscard]] GpuNativeTarget Native(GpuTarget target) const override;

    void BeginFrame() override;
    void EndFrame() override;
    void SetBackBufferSize(int width, int height) override { _backBuffer = {0, 0, width, height}; }
    [[nodiscard]] PixelRect BackBuffer() const { return _backBuffer; }
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

    // Recorded calls since construction or the last ClearCommands.
    [[nodiscard]] const std::vector<NullCommand>& Commands() const { return _commands; }
    void ClearCommands() { _commands.clear(); }
    [[nodiscard]] std::vector<const NullCommand*> CommandsOf(NullCommandType type) const;
    // The vertices a DrawIndexed command drew, in index order.
    [[nodiscard]] std::vector<PaintVertex> DrawnVertices(const NullCommand& draw) const;
    [[nodiscard]] const GpuTextureDesc& Describe(GpuTexture texture) const;
    [[nodiscard]] const GpuTargetDesc& Describe(GpuTarget target) const;
    [[nodiscard]] const std::vector<std::byte>& Pixels(GpuTexture texture) const;
    [[nodiscard]] std::size_t LiveTextures() const;
    [[nodiscard]] std::size_t LiveTargets() const;
    [[nodiscard]] int Frame() const { return _frame; }

private:
    struct TextureSlot {
        std::uint32_t generation = 0;
        bool live = false;
        bool owned = true; // False for a target's attachments (destroyed with the target)
        GpuTextureDesc desc;
        std::vector<std::byte> pixels;
    };
    struct TargetSlot {
        std::uint32_t generation = 0;
        bool live = false;
        GpuTargetDesc desc;
        GpuTexture color;
        GpuTexture emission;
    };
    [[nodiscard]] GpuTexture AddTexture(const GpuTextureDesc& desc, bool owned);
    [[nodiscard]] TextureSlot& Live(GpuTexture texture);
    [[nodiscard]] const TextureSlot& Live(GpuTexture texture) const;
    [[nodiscard]] const TargetSlot& Live(GpuTarget target) const;
    void Record(NullCommand command) { _commands.push_back(std::move(command)); }

    std::vector<TextureSlot> _textures{1}; // Slot 0 is the null handle
    std::vector<TargetSlot> _targets{1};
    std::vector<PaintVertex> _vertices; // Everything uploaded, never recycled
    std::vector<std::uint32_t> _indices;
    std::vector<NullCommand> _commands;
    GpuFrameCounters _counters;
    PixelRect _backBuffer;
    bool _inFrame = false;
    bool _inPass = false;
    int _markerDepth = 0;
    int _frame = 0;
};
