#pragma once
#include "Editor/Api/PreviewRegistry.h"

// Engine preview of every Painter feature (shapes, lines, glows, paths, text, scopes, surfaces, post-processing),
// for checking the renderer visually without a game.
[[nodiscard]] PreviewDefinition RenderGalleryDefinition();
