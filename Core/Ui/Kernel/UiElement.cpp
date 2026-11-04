#include "Ui/Kernel/UiElement.h"
#include <stdexcept>
#include "Ui/Kernel/Internals/UiKernel.h"

namespace {
[[nodiscard]] UiKernel& Open(UiKernel* kernel) {
    if (!kernel) throw std::logic_error("UiElement: used after it was closed");
    return *kernel;
}
} // namespace

UiElement::~UiElement() {
    if (_kernel) _kernel->CloseElement(_node);
}
void UiElement::Close() {
    if (_kernel) _kernel->CloseElement(_node);
    _kernel = nullptr;
}
UiId UiElement::Id() const { return Open(_kernel).IdOf(_node); }
bool UiElement::Hovered() const { return Open(_kernel).Hovered(_node); }
bool UiElement::Pressed() const { return Open(_kernel).Pressed(_node); }
bool UiElement::Focused(int user) const { return Open(_kernel).Focused(_node, user); }
UiAction UiElement::Activated() const { return Open(_kernel).Activated(_node); }
bool UiElement::Denied() const { return Open(_kernel).Denied(_node); }
void UiElement::TakeFocus(int user) { Open(_kernel).TakeFocus(_node, user); }
FloatRect UiElement::LastRect() const { return Open(_kernel).LastRect(_node); }
float UiElement::Age() const { return Open(_kernel).Age(_node); }
void UiElement::Draw(UiDrawFunction draw) { Open(_kernel).SetDraw(_node, std::move(draw)); }
void UiElement::SetVisual(const UiVisual& visual) { Open(_kernel).SetVisual(_node, visual); }
void* UiElement::FindState(std::type_index type) const { return Open(_kernel).FindState(_node, type); }
void* UiElement::AddState(std::type_index type, std::shared_ptr<void> state) {
    return Open(_kernel).AddState(_node, type, std::move(state));
}
