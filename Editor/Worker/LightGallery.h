#pragma once
#include "Editor/Api/SceneRegistry.h"

// Engine scene of the light layer: point lights, cones and cores over a coloured floor, what shines by itself before
// and after the layer, a turning and zooming camera and a split screen, for checking lighting visually without a game.
[[nodiscard]] SceneDefinition LightGalleryDefinition();
