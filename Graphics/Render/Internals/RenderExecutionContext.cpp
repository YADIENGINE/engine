#include "Graphics/Render/Internals/RenderExecutionContext.h"

namespace {
thread_local RenderExecutionContext* ActiveContext = nullptr;
}

RenderExecutionContext::RenderExecutionContext(const RenderExecutionOptions& options, FrameObserver* observer)
    : _options(options)
    , _observer(observer)
    , _previous(ActiveContext) {
    ActiveContext = this;
}
RenderExecutionContext::~RenderExecutionContext() { ActiveContext = _previous; }
GlowBatching RenderExecutionContext::Glows() {
    return ActiveContext ? ActiveContext->_options.glowBatching : GlowBatching::Batched;
}
void RenderExecutionContext::Count(RenderCounter counter, std::uint64_t amount) {
    if (ActiveContext && ActiveContext->_observer) ActiveContext->_observer->CountRender(counter, amount);
}
