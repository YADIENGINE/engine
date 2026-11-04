#pragma once
#include "Graphics/Style/NeonArcadeTheme.h"
#include "Graphics/Style/Theme.h"

// The default look: quiet greys and one blue accent, nothing moving in the background. It is meant to be replaced, and
// to stay readable until then. Effects are the same as NeonArcadeTheme's.
inline constexpr ThemePalette PlainPalette{.background = {28, 30, 34, 255},
                                           .surface = {40, 43, 49, 255},
                                           .surfaceFrame = {88, 93, 103, 255},
                                           .text = {240, 241, 243, 255},
                                           .textSoft = {190, 193, 199, 255},
                                           .textDim = {135, 139, 147, 255},
                                           .overlay = {0, 0, 0, 150},
                                           .accent = {92, 160, 255, 255},
                                           .warning = {232, 84, 72, 255}};

inline constexpr Theme PlainTheme{.palette = PlainPalette, .fx = NeonArcadeFx, .backdrop = {}};
