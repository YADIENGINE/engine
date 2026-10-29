#pragma once
#include <cstdint>
#include "Graphics/Render/RenderExecutionOptions.h"
#include "Utilities/FrameObserver.h"

// Renderer installs a borrowed context for one frame; nested render layers inherit it.
class RenderExecutionContext {
public:
    RenderExecutionContext(const RenderExecutionOptions& options, FrameObserver* observer);
    ~RenderExecutionContext();
    [[nodiscard]] static GlowBatching Glows();
    static void Count(RenderCounter counter, std::uint64_t amount);

private:
    const RenderExecutionOptions& _options;
    FrameObserver* _observer;
    RenderExecutionContext* _previous;

public:
    RenderExecutionContext(const RenderExecutionContext&) = delete;
    RenderExecutionContext& operator=(const RenderExecutionContext&) = delete;
};
