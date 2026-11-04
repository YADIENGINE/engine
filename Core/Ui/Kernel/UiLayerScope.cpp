#include "Ui/Kernel/UiLayerScope.h"
#include <utility>
#include "Ui/Kernel/Internals/UiKernel.h"

UiLayerScope::UiLayerScope(UiKernel& kernel, UiId id, bool open, const UiLayerSpec& spec)
    : _kernel(&kernel)
    , _closing(!open) {
    if (!kernel.OpenLayer(id, open, spec, _progress, _root, _savedCursor, _savedLayer)) _kernel = nullptr;
}
UiLayerScope::~UiLayerScope() {
    if (_kernel) _kernel->CloseLayer(_savedCursor, _savedLayer);
}
void UiLayerScope::Draw(UiDrawFunction draw) {
    if (_kernel) _kernel->SetDraw(_root, std::move(draw));
}
void UiLayerScope::SetVisual(const UiVisual& visual) {
    if (_kernel) _kernel->SetVisual(_root, visual);
}
