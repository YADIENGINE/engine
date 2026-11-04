#pragma once
#include "Animation/ButtonAnim.h"
#include "Graphics/Draw/Neon.h"
#include "Ui/Kernel/UiLooks.h"

// Notched dark panels, glowing borders and 3D buttons that depress: goes with NeonArcadeTheme.
//     settings.theme = NeonArcadeTheme;
//     settings.uiLooks = NeonLooks();
[[nodiscard]] UiLooks NeonLooks();

// The styles NeonLooks draws with, for drawing the same widgets by hand (Graphics/Draw/Neon.h).
struct NeonUiStyle {
    NeonFrameStyle frame;
    NeonTextStyle text;
    NeonPanelStyle panel;
    NeonChipStyle chip;
    ArcadeButtonStyle button;
    RoundButtonStyle roundButton;
    KeyCapStyle keyCap;
    ProgressBarStyle progressBar;
    ButtonAnimSettings buttonAnimation;
};
[[nodiscard]] const NeonUiStyle& NeonUiStyles();
