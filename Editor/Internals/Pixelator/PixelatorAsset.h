#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Vec2.h"

// The Pixelator's copy of a pixel graphic (GraphicAsset::pixels), as the worker sends it in its "pixels" message
// (Docs/Design/README.md → D64, D65). Plain data, so an editing tool can later change it and send it back.

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
    std::string error;                // Set when the graphic could not be read; nothing else is then
    std::vector<PixelatorItem> items; // Every item of the set, empty for a single drawing
    std::size_t item = 0;             // Index into items
    std::vector<PixelatorImage> images;
    std::vector<std::vector<Color4>> palettes; // Each with ink 0 (transparent) first, as PixelPalette::Colors()
    std::vector<PixelatorFrame> frames;
    bool loops = true;    // The frames repeat (GraphicPixels::loops)
    bool capped = false;  // Frames past the size limit were left out
    std::vector<std::string> inkNames;  // By ink number (index 0 unused); shorter or empty when not named
    std::vector<std::string> partNames; // By part number (index 0: no part)
    std::vector<InkRamp> ramps;         // The registration's shading ramps, for lint
    bool operator==(const PixelatorAsset&) const = default;

    // An ink's or a part's registered name; empty when it has none.
    [[nodiscard]] const std::string& InkName(int ink) const { return NameIn(inkNames, ink); }
    [[nodiscard]] const std::string& PartName(int part) const { return NameIn(partNames, part); }

private:
    [[nodiscard]] static const std::string& NameIn(const std::vector<std::string>& names, int index) {
        static const std::string None;
        return index > 0 && static_cast<std::size_t>(index) < names.size() ? names[static_cast<std::size_t>(index)]
                                                                           : None;
    }
};

// "5", or "5 Steel1" when the ink has a name.
[[nodiscard]] std::string PixelatorInkLabel(const PixelatorAsset& asset, int ink);
