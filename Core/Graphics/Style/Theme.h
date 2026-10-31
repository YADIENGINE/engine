#pragma once
#include "Fx/FxStyle.h"
#include "Graphics/Anim/ButtonAnim.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Draw/Neon.h"
#include "Graphics/Style/ThemePalette.h"

// UI widget styles (read by Ui/UiDraw and Ui/UiButton).
struct UiStyle {
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

// Screen background roles (read by Ui/Backdrop): menus, surroundings of the game field, preview / gallery screens;
// scanline geometry is shared by all.
struct BackdropTheme {
    BackdropStyle menu;
    BackdropStyle play;
    BackdropStyle gallery;
    ScanlineStyle scanlines;
};

// The game's visual identity, selected through GameSettings::theme (default NeonArcadeTheme) and activated by the
// engine before scenes are created. Themed drawing (DrawUi*) reads the active theme on each call; for a one-off
// variant, copy the style and pass it to a drawing function taking an explicit style (DrawNeon*, DrawArcadeButton).
// Fx does not read the theme: FxStage and individual effect systems receive styles explicitly from 'fx'.
struct Theme {
    ThemePalette palette;
    UiStyle ui;
    FxStyle fx;
    BackdropTheme backdrop;
};

[[nodiscard]] const Theme& CurrentTheme();
void SetTheme(const Theme& theme);
