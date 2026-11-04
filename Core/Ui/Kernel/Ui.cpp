#include "Ui/Kernel/Ui.h"
#include <utility>
#include "Ui/Kernel/Internals/UiKernel.h"
#include "Ui/Kernel/UiListener.h"

Ui::Ui(const UiSettings& settings)
    : _kernel(std::make_unique<UiKernel>(settings)) {}
Ui::~Ui() = default;

void Ui::SetCanvas(const FloatRect& canvas) { _kernel->settings.canvas = canvas; }
const FloatRect& Ui::Canvas() const { return _kernel->settings.canvas; }
void Ui::SetFont(const FontFace* font) { _kernel->settings.font = font; }
void Ui::SetUsers(std::vector<UiKeys> users) { _kernel->SetUsers(std::move(users)); }
int Ui::UserCount() const { return _kernel->UserCount(); }
UiLooks& Ui::Looks() { return _kernel->looks; }
const UiLooks& Ui::Looks() const { return _kernel->looks; }
void Ui::Input(const DeviceInput& input) { _kernel->Input(input); }
void Ui::CancelInput() { _kernel->CancelInput(); }
void Ui::Begin(float seconds) { _kernel->Begin(seconds); }
void Ui::End() {
    _kernel->End();
    if (UiListener* listener = ActiveUiListener()) listener->FrameBuilt(*this);
}
void Ui::Draw(Painter& paint) const { _kernel->Draw(paint); }
UiElement Ui::Element(UiId id, const UiBox& box, const UiTraits& traits) {
    return UiElement(*_kernel, _kernel->OpenElement(id, box, traits));
}
UiElement Ui::Element(const UiBox& box, const UiTraits& traits) {
    return UiElement(*_kernel, _kernel->OpenElement(std::nullopt, box, traits));
}
UiLayerScope Ui::Layer(UiId id, bool open, const UiLayerSpec& spec) {
    return UiLayerScope(*_kernel, id, open, spec);
}
UiAction Ui::Back() { return _kernel->Back(); }
int Ui::PageStep() const { return _kernel->PageStep(); }
std::optional<Float2> Ui::Pointer() const { return _kernel->Pointer(); }
float Ui::Wheel() const { return _kernel->Wheel(); }
bool Ui::CursorMovedByKeys() const { return _kernel->CursorMovedByKeys(); }
std::string_view Ui::Text(std::string_view text) { return _kernel->CopyText(text); }
const FontFace& Ui::Font() const { return _kernel->Font(); }
float Ui::Delta() const { return _kernel->Delta(); }
std::span<const UiEvent> Ui::Events() const { return _kernel->Events(); }
const UiStats& Ui::Stats() const { return _kernel->stats; }
