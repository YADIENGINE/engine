#pragma once
#include "Graphics/Draw/BackgroundDraw.h"
#include "Utilities/FloatRect.h"

class Painter;

// The theme's background roles (CurrentTheme().backdrop). Menu: menu screens; Play: around the play field;
// Gallery: editor scene and gallery screens.
enum class BackdropRole { Menu, Play, Gallery };

[[nodiscard]] const BackdropStyle& ThemeBackdrop(BackdropRole role);

// Grid, vignette and scanlines are separate calls; the view chooses their order. A background outside the theme (a
// game's map style) passes its BackdropStyle to Graphics/Draw/BackgroundDraw.h directly.
void DrawBackdropGrid(Painter& paint, BackdropRole role, const FloatRect& area);
// Draws nothing for roles without a vignette.
void DrawBackdropVignette(Painter& paint, BackdropRole role, const FloatRect& area);
void DrawBackdropScanlines(Painter& paint, BackdropRole role, const FloatRect& area);
// The theme's scanlines at 'alpha', for layers over a scene (a card screen).
void DrawScanlineOverlay(Painter& paint, const FloatRect& area, float alpha);
