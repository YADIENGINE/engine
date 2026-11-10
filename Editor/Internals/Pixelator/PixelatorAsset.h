#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "Graphics/Pixel/PixelCanvas.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Vec2.h"

// The Pixelator's copy of a pixel graphic (GraphicAsset::pixels), as the worker sends it in its "pixels" message
// (Docs/Design/README.md → D64). Plain data, so an editing tool can later change it and send it back.

// One distinct sprite: inks and parts, rows top to bottom. An empty sprite has no pixels.
struct PixelatorImage {
    int width = 0;
    int height = 0;
    Vec2 pivot;
    std::vector<Ink> inks;
    std::vector<PixelPart> parts;

    [[nodiscard]] bool Empty() const { return width <= 0 || height <= 0; }
    [[nodiscard]] Ink InkAt(int x, int y) const { return inks[static_cast<std::size_t>(y * width + x)]; }
    [[nodiscard]] PixelPart PartAt(int x, int y) const { return parts[static_cast<std::size_t>(y * width + x)]; }
    bool operator==(const PixelatorImage&) const = default;
};

// One step of the animation: which sprite in which palette, shown for `seconds`.
struct PixelatorFrame {
    std::size_t image = 0;
    std::size_t palette = 0;
    float seconds = 0.0f;
    bool operator==(const PixelatorFrame&) const = default;
};

struct PixelatorItem {
    std::string name;
    std::string id; // Its catalog entry
    bool operator==(const PixelatorItem&) const = default;
};

struct PixelatorAsset {
    std::string id;                   // The catalog entry shown
    std::string error;                // Set when the graphic could not be sampled; nothing else is then
    std::vector<PixelatorItem> items; // Every item of the set, empty for a single drawing
    std::size_t item = 0;             // Index into items
    std::vector<PixelatorImage> images;
    std::vector<std::vector<Color4>> palettes; // Each with ink 0 (transparent) first, as PixelPalette::Colors()
    std::vector<PixelatorFrame> frames;
    float window = 0.0f; // Seconds of the graphic's clock that were sampled
    bool looped = false; // The frames are one cycle that repeats
    bool capped = false; // The frame, size or time limit ended sampling early
    bool operator==(const PixelatorAsset&) const = default;
};
