#pragma once
#include "Editor/Api/SceneRegistry.h"

// Engine scene of every Painter feature (shapes, lines, glows, paths, text, scopes, surfaces, post-processing),
// for checking the renderer visually without a game.
[[nodiscard]] SceneDefinition RenderGalleryDefinition();
