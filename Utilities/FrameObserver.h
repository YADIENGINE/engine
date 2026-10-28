#pragma once
#include <cstdint>

enum class FrameRegion { Input, Audio, Application, Scene, Canvas, PreviewBridge, Present, EndDrawing, Count };
enum class RenderCounter { Glows, GlowBlendRequests };

// Optional frame instrumentation. The observer outlives its attachment; callbacks run on the game thread.
// Engine owns no clock or sample storage for observers. Callbacks must not throw or change drawing state.
class FrameObserver {
public:
    virtual ~FrameObserver() = default;
    virtual void BeginFrame() noexcept = 0;
    virtual void EndFrame() noexcept = 0;
    virtual void BeginRegion(FrameRegion region) noexcept = 0;
    virtual void EndRegion(FrameRegion region) noexcept = 0;
    virtual void CountRender(RenderCounter counter, std::uint64_t amount) noexcept = 0;
};
