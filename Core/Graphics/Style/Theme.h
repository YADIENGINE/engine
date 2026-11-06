#pragma once
#include "Fx/FxStyle.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Style/ThemePalette.h"

// Screen background roles (read by Graphics/Backgrounds/Backdrops): menus, surroundings of the game field, catalog scene / gallery screens;
// scanline geometry is shared by all.
struct BackdropTheme {
    BackdropStyle menu;
    BackdropStyle play;
    BackdropStyle gallery;
    ScanlineStyle scanlines;
};

// The game's colours, backgrounds and effects, selected through GameSettings::theme (default PlainTheme) and activated
// by the engine before scenes are created. How widgets look is separate: see Ui/Kernel/UiLooks.h. Looks read the palette
// on each call. Fx does not read the theme: FxStage and individual effect systems receive styles explicitly from 'fx'.
struct Theme {
    ThemePalette palette;
    FxStyle fx;
    BackdropTheme backdrop;
};

[[nodiscard]] const Theme& CurrentTheme();
void SetTheme(const Theme& theme);
