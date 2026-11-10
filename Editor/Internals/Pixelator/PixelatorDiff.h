#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Vec2.h"

// What changed in a pixel graphic between the build before the last Build & Refresh and this one (D65): palette
// colours, frame count and durations, and per frame the size, pivot and the pixels whose ink differs. Frames are
// matched by number and sprites aligned on their pivots, so a sprite that grew on one side does not count as moved.

struct PixelatorPaletteChange {
    int ink = 0;
    Color4 before; // Transparent when the ink is new
    Color4 after;  // Transparent when the ink is gone
    bool operator==(const PixelatorPaletteChange&) const = default;
};

struct PixelatorFrameChange {
    std::size_t frame = 0;
    bool added = false;   // No such frame before
    bool removed = false; // No such frame now
    Vec2 sizeBefore, sizeAfter;
    Vec2 pivotBefore, pivotAfter;
    std::optional<float> secondsBefore, secondsAfter;
    // Pixels whose ink differs, in the current frame's coordinates (outside it for pixels only the old sprite had).
    std::vector<Vec2> pixels;
    int partsOnly = 0; // Pixels with the same ink and another part
    bool operator==(const PixelatorFrameChange&) const = default;
};

struct PixelatorDiff {
    std::vector<PixelatorPaletteChange> palette;
    std::vector<PixelatorFrameChange> frames; // Only frames with a change
    std::size_t framesBefore = 0;
    std::size_t framesAfter = 0;
    [[nodiscard]] bool Same() const { return palette.empty() && frames.empty() && framesBefore == framesAfter; }
    [[nodiscard]] std::size_t ChangedPixels() const;
    // The change of one frame of the current build, or null when it did not change.
    [[nodiscard]] const PixelatorFrameChange* Frame(std::size_t frame) const;
};

[[nodiscard]] PixelatorDiff DiffPixelator(const PixelatorAsset& before, const PixelatorAsset& after);

inline constexpr Color4 PixelatorDiffRemoved{255, 64, 200, 230}; // A pixel the old sprite had and the new one lacks
inline constexpr std::uint8_t PixelatorDiffDimAlpha = 56;       // Of unchanged pixels

// The Diff view's colours for a frame of the current build: changed pixels in their colour, pixels that became clear
// in PixelatorDiffRemoved, unchanged ones dimmed. `change` null: everything dimmed.
[[nodiscard]] std::vector<Color4> PixelatorDiffColors(const PixelatorImage& image, std::span<const Color4> palette,
                                                      const PixelatorFrameChange* change);
