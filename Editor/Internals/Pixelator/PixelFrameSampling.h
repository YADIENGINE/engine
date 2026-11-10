#pragma once
#include <cstddef>
#include <functional>
#include "Editor/Api/GraphicAsset.h"
#include "Editor/Internals/Pixelator/PixelatorAsset.h"

// How far the worker samples a pixel graphic's clock for the Pixelator's frames.
struct PixelSamplingLimits {
    float window = 4.0f;                    // Seconds of the graphic's clock
    int rate = 60;                          // Samples per second
    std::size_t maxFrames = 64;             // Frames kept before sampling stops (capped)
    std::size_t maxBytes = 6 * 1024 * 1024; // Inks + parts of all distinct sprites
    double maxWallSeconds = 3.0;            // Real time the sampling may take
};

// The frames of a graphic whose API only gives "the sprite at a time" (GraphicAsset::pixels). Samples `pixels` at
// t = k / rate over [0, window), keeps each distinct sprite (same id, or same size, pivot, inks and parts) and palette
// once, and merges consecutive equal samples into one frame with their summed duration. When the frames repeat a
// shorter cycle (the repeat covering at least two frames and a quarter of the cycle; the last frame may be cut by the
// window), only that cycle is kept (`looped`). Limits: durations are whole samples (1/60 s), a change faster than the
// rate can be missed, a cycle longer than about 3 s of a 4 s window shows as one unlooped pass, and
// the frame, size and time limits end sampling early (`capped`). A later explicit frames API on GraphicAsset replaces
// this. Fills images, palettes, frames, window, looped and capped; an exception from `pixels` propagates.
[[nodiscard]] PixelatorAsset SamplePixelFrames(const std::function<GraphicPixels(float time)>& pixels,
                                               const PixelSamplingLimits& limits = {});
// The sprite as the Pixelator holds it.
[[nodiscard]] PixelatorImage PixelatorImageOf(const PixelSprite& sprite);
