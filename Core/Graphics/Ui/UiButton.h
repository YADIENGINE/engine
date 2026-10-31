#pragma once
#include "Graphics/Anim/ButtonAnim.h"
#include "Graphics/Draw/Neon.h"

// Selection spring, press and rejection shake using the theme's button animation (CurrentTheme().ui.buttonAnimation).
// Look supplies the appearance expected by DrawUiButton. Stored as a view's value member.
class UiButton {
public:
    UiButton();
    void Update(bool selected, float deltaSeconds);
    void Press();
    void Deny();
    void Clear();
    [[nodiscard]] ButtonLook Look(Color4 accent) const;

private:
    ButtonAnim _animation;
};
