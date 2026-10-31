#pragma once
#include "Graphics/Style/ThemePalette.h"

// Role colors for the neon arcade look: dark purple background, green accent.
inline constexpr ThemePalette NeonArcadePalette{.background = {11, 9, 26, 255},
                                                .surface = {18, 14, 40, 250},
                                                .surfaceFrame = {70, 66, 100, 255},
                                                .text = {255, 255, 255, 255},
                                                .textSoft = {190, 186, 220, 255},
                                                .textDim = {155, 150, 190, 255},
                                                .overlay = {0, 0, 0, 160},
                                                .accent = {60, 255, 120, 255},
                                                .warning = {255, 50, 60, 255}};
