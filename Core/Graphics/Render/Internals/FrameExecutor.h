#pragma once
#include <optional>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Paint/Internals/DrawList.h"

// Turns recorded drawing into device calls. Target flushes and native tool overlays may split a frame into
// segments; open targets carry over. Redundant pipeline, scissor and texture changes are skipped.
class FrameExecutor {
public:
    explicit FrameExecutor(GpuDevice& device);

    // Executes everything 'list' recorded since the previous call.
    void Execute(DrawList& list);
    // Between frames: no target may still be open.
    [[nodiscard]] bool Idle() const { return _targets.empty(); }
    [[nodiscard]] std::uint32_t BatchesExecuted() const { return _batches; }
    void ResetCounters() { _batches = 0; }

private:
    void Begin(const PaintTarget& target, std::optional<PremultipliedColor> clear);
    void ResetCache();

    GpuDevice& _device;
    std::vector<PaintTarget> _targets;
    bool _passOpen = false;
    std::optional<GpuBlend> _blend;
    std::optional<std::optional<PixelRect>> _scissor;
    std::vector<GpuTexture> _textures;
    std::vector<std::uint32_t> _indices; // Rebased indices of the segment being uploaded
    bool _texturesKnown = false;
    std::uint32_t _batches = 0;

public:
    FrameExecutor(const FrameExecutor&) = delete;
    FrameExecutor& operator=(const FrameExecutor&) = delete;
};
