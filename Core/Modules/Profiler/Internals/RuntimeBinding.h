#pragma once
#include <array>
#include "Modules/Profiler/Internals/RuntimeState.h"
#include "Modules/Profiler/Internals/GpuTimer.h"
#include "Utilities/FrameObserver.h"

class ProfilerRuntimeBinding final : public Profiler::Runtime, public FrameObserver {
public:
    ProfilerRuntimeBinding(Profiler& profiler, Game& game);
    ~ProfilerRuntimeBinding() override;
    void BeginFrame() noexcept override;
    void EndFrame() noexcept override;
    void BeginRegion(FrameRegion region) noexcept override;
    void EndRegion(FrameRegion region) noexcept override;
    void CountRender(RenderCounter counter, std::uint64_t amount) noexcept override;
    void Poll() override;
    [[nodiscard]] bool Supported() const override { return _gpu.Supported(); }
    [[nodiscard]] std::size_t Pending() const override { return _gpu.Pending(); }
    [[nodiscard]] Profiler::Metadata MetadataValue() const override;

private:
    Profiler& _profiler;
    Game& _game;
    GpuTimer _gpu;
    std::array<Profiler::ScopeId, static_cast<std::size_t>(FrameRegion::Count)> _regions{};
    std::array<Profiler::ScopeId, 2> _counters{};
    bool _active = false;
};
