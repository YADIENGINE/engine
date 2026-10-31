#pragma once
#include "Graphics/Anim/ButtonAnim.h"
#include "Graphics/Draw/Neon.hpp"

// Temanın buton animasyonuyla (CurrentTheme().ui.buttonAnimation) seçim yayı, basış ve ret sarsıntısı; Look,
// DrawUiButton'ın beklediği görünümü verir. View'ın değer üyesidir.
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
