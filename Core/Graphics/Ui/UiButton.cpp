#include "Graphics/Ui/UiButton.h"
#include "Graphics/Style/Theme.h"

UiButton::UiButton()
    : _animation(CurrentTheme().ui.buttonAnimation) {}
void UiButton::Update(bool selected, float deltaSeconds) { _animation.Update(selected, deltaSeconds); }
void UiButton::Press() { _animation.Press(); }
void UiButton::Deny() { _animation.Deny(); }
void UiButton::Clear() { _animation.Clear(); }
ButtonLook UiButton::Look(Color4 accent) const { return {.animation = _animation.State(), .accent = accent}; }
