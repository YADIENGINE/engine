#pragma once
#include "Editor/Api/SceneRegistry.h"

// Engine scene of the light layer: point lights, cones and cores over a coloured floor, shadows from colliders and from
// what moves, what shines by itself before and after the layer, a turning and zooming camera and a split screen, for
// checking lighting visually without a game.
[[nodiscard]] SceneDefinition LightGalleryDefinition();
