#pragma once
#include <raylib.h>
#include "Graphics/Anim/ButtonAnim.h"
#include "Graphics/Draw/Neon.h"

// Temanın buton animasyonuyla (CurrentTheme().ui.buttonAnimation) seçim yayı, basış ve ret sarsıntısı; Look,
// DrawUiButton'ın beklediği görünümü verir. View'ın değer üyesidir.
class UiButton {
public:
    UiButton();
    void Update(bool selected, float deltaSeconds);
    void Press();
    void Deny();
    void Clear();
    [[nodiscard]] ButtonLook Look(Color accent) const;

private:
    ButtonAnim _animation;
};
