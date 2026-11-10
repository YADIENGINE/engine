#pragma once
#include <cstddef>
#include <functional>
#include <source_location>
#include <string>
#include <vector>
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

struct GraphicItem {
    std::string name;
    std::string tags; // Extra search words
};

// One item of a pixel-art graphic as data: what GraphicAsset::pixels returns.
struct GraphicPixels {
    PixelSprite sprite;
    PixelPalette palette;
};

// A ready-made graphic for the editor's Assets panel. Every graphic is a set; a single drawing leaves `items` empty
// and takes its name from the file. Keep registration in `X.asset.cpp` under an `Assets/Graphics` folder: only the
// editor worker compiles those files, so games never carry the catalog.
struct GraphicAsset {
    std::vector<GraphicItem> items;
    // Draws one item fitted to `box`; `item` indexes `items` (0 for a single drawing). `time` is the scene's clock.
    std::function<void(Painter& paint, const FloatRect& box, std::size_t item, float time)> draw;
    // For pixel art: the sprite and palette an item shows at `time`. Opening the graphic then brings the editor's
    // Pixelator panel forward (Docs/Graphics/PixelArt.md#in-the-editor): zoom, Color / Inks / Parts views, grids, the
    // pivot, the inks it uses and its frames, which the editor finds by sampling `time` from 0 to 4 s (keep it a pure
    // function of item and time). `python Engine ctl pixels` shows agents the same. Optional: `draw` still makes the
    // thumbnail and what the Viewport shows.
    std::function<GraphicPixels(std::size_t item, float time)> pixels;
    // The moment the thumbnail shows, for a graphic that starts empty or looks best mid-animation.
    float thumbnailTime = 0.0f;
    // The line "Copy usage" copies; `{item}` becomes the item's name.
    std::string usage;
};

// Stores the maker only; it runs when the worker builds its catalog. Use at the end of the file, in an anonymous
// namespace: [[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeX);
bool RegisterGraphicAsset(GraphicAsset (*make)(), std::source_location where = std::source_location::current());
