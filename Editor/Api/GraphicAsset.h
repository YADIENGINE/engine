#pragma once
#include <cstddef>
#include <functional>
#include <source_location>
#include <string>
#include <vector>
#include "Graphics/Pixel/PixelAnimation.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/Pixel/PixelSpriteSet.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

struct GraphicItem {
    std::string name;
    std::string tags; // Extra search words
};

// A pixel-art item as data for the editor's Pixelator (GraphicAsset::pixels): every frame it shows, in order, with
// its duration, and the palette. Fill it from the same frames the game draws, so the editor never guesses:
// GraphicPixels::Animated(sprites, animation, palette) for a PixelSpriteSet played by a PixelAnimation,
// GraphicPixels::Still(sprite, palette) for one sprite. Names and ramps are optional and cost the game nothing (only
// the editor's registration fills them).
struct GraphicPixels {
    struct Frame {
        PixelSprite sprite;
        float seconds = 0.1f; // How long it shows; above 0
    };
    std::vector<Frame> frames;
    PixelPalette palette;
    bool loops = true; // The frames repeat; false: the last one stays
    // Names of inks 1, 2, 3, ... in palette order, as the game's ink enum lists them ("Line", "Steel0", ...);
    // `ctl pixels` reports them with each ink.
    std::vector<std::string> inkNames;
    // Names of parts 1, 2, 3, ... (PixelCanvas::Part numbers).
    std::vector<std::string> partNames;
    // The ramps the drawing shades with (PixelCanvas::Shade); lint checks that each step is lighter and visible.
    std::vector<InkRamp> ramps;

    // One frame per step of `animation`: sprites[step.sprite] for step.seconds; it loops when the animation does.
    [[nodiscard]] static GraphicPixels Animated(const PixelSpriteSet& sprites, const PixelAnimation& animation,
                                                PixelPalette palette);
    // A single frame.
    [[nodiscard]] static GraphicPixels Still(PixelSprite sprite, PixelPalette palette);
};

// A ready-made graphic for the editor's Assets panel. Every graphic is a set; a single drawing leaves `items` empty
// and takes its name from the file. Keep registration in `X.asset.cpp` under an `Assets/Graphics` folder: only the
// editor worker compiles those files, so games never carry the catalog.
struct GraphicAsset {
    std::vector<GraphicItem> items;
    // Draws one item fitted to `box`; `item` indexes `items` (0 for a single drawing). `time` is the scene's clock.
    std::function<void(Painter& paint, const FloatRect& box, std::size_t item, float time)> draw;
    // For pixel art: an item's frames and palette. Opening the graphic then brings the editor's Pixelator panel
    // forward (Docs/Graphics/PixelArt.md#in-the-editor): zoom, Color / Inks / Parts views, grids, the pivot, the inks it
    // uses and its frames at their durations. `python Engine ctl pixels` shows agents the same. Optional: `draw` still
    // makes the thumbnail and what the Viewport shows.
    std::function<GraphicPixels(std::size_t item)> pixels;
    // The moment the thumbnail shows, for a graphic that starts empty or looks best mid-animation.
    float thumbnailTime = 0.0f;
    // The line "Copy usage" copies; `{item}` becomes the item's name.
    std::string usage;
};

// Stores the maker only; it runs when the worker builds its catalog. Use at the end of the file, in an anonymous
// namespace: [[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeX);
bool RegisterGraphicAsset(GraphicAsset (*make)(), std::source_location where = std::source_location::current());
