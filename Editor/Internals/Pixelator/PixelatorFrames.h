#pragma once
#include <cstddef>
#include "Editor/Api/GraphicAsset.h"
#include "Editor/Internals/Pixelator/PixelatorAsset.h"

// Most bytes of sprite data (inks + parts of the distinct sprites) one "pixels" message carries; frames past it are
// left out (`capped`). Base64 makes the message a third larger, within the worker's 16 MB.
inline constexpr std::size_t PixelatorMaxSpriteBytes = 6 * 1024 * 1024;

// The Pixelator's copy of what a registration declared (GraphicAsset::pixels): each distinct sprite once (same id, or
// same size, pivot, inks and parts), the palette, the frames with their durations, loops, names and ramps. Throws
// invalid_argument for a frame shorter than 0 s or without a duration, or more than 1024 frames. Fills images,
// palettes, frames, loops, capped, inkNames, partNames and ramps.
[[nodiscard]] PixelatorAsset PixelatorFramesOf(const GraphicPixels& pixels,
                                               std::size_t maxBytes = PixelatorMaxSpriteBytes);
// The sprite as the Pixelator holds it.
[[nodiscard]] PixelatorImage PixelatorImageOf(const PixelSprite& sprite);
