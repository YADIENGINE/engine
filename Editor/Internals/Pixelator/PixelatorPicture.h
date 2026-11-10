#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Editor/Internals/Session/FrameCapture.h"

// How the Pixelator colours a sprite and what it draws around it. The panel draws with these on the GPU; the control
// endpoint's `pixels` command composes the same picture on the CPU (ComposePixelatorPicture).

// Color: the palette's colours. Inks / Parts: a distinct, readable colour per ink or per part.
enum class PixelatorViewMode { Color, Inks, Parts };
enum class PixelatorBackground { Checker, Dark, Light };

inline constexpr Color4 PixelatorCheckerLight{58, 62, 70, 255};
inline constexpr Color4 PixelatorCheckerDark{46, 50, 57, 255};
inline constexpr Color4 PixelatorDarkBackground{22, 24, 28, 255};
inline constexpr Color4 PixelatorLightBackground{214, 217, 222, 255};
inline constexpr Color4 PixelatorGridColor{0, 0, 0, 80};
inline constexpr Color4 PixelatorTileColor{255, 176, 64, 170};
inline constexpr Color4 PixelatorPivotColor{255, 80, 200, 255};
inline constexpr int PixelatorCheckerCell = 16;       // Screen pixels at least; a whole number of sprite pixels
inline constexpr std::uint8_t PixelatorDimAlpha = 40; // Of inks outside the focused one
inline constexpr int PixelatorPictureMaxSide = 8192;  // Pixels of a composed picture
inline constexpr int PixelatorPictureAutoSide = 1024; // The long side an automatic zoom aims at

// A distinct colour per index (golden-ratio hues), readable on dark and light backgrounds.
[[nodiscard]] Color4 PixelatorIndexColor(int index);

struct PixelatorCount {
    int index = 0; // Ink or part
    int pixels = 0;
    bool operator==(const PixelatorCount&) const = default;
};
// The inks a sprite uses (not ink 0), in ink order, with their pixel counts.
[[nodiscard]] std::vector<PixelatorCount> PixelatorInkCounts(const PixelatorImage& image);
// The parts of its drawn pixels, part 0 (none) included when drawn pixels have no part.
[[nodiscard]] std::vector<PixelatorCount> PixelatorPartCounts(const PixelatorImage& image);

// One colour per pixel, rows top to bottom. `focus`: the other inks keep only PixelatorDimAlpha of their alpha.
// Palettes missing an ink show it transparent.
[[nodiscard]] std::vector<Color4> PixelatorColors(const PixelatorImage& image, std::span<const Color4> palette,
                                                  PixelatorViewMode mode, std::optional<Ink> focus = std::nullopt);

[[nodiscard]] Color4 PixelatorBackgroundColor(PixelatorBackground background, bool alternate = false);
// Screen pixels per checker cell at a zoom: a whole number of sprite pixels, at least PixelatorCheckerCell.
[[nodiscard]] int PixelatorCheckerSize(int zoom);
// The inner tile lines across `length` sprite pixels: every multiple of `tile` strictly between 0 and length.
[[nodiscard]] std::vector<int> PixelatorTileLines(int length, int tile);

struct PixelatorPictureOptions {
    PixelatorViewMode view = PixelatorViewMode::Color;
    int zoom = 0;             // 0: automatic (PixelatorPictureZoom)
    std::optional<bool> grid; // Unset: when the zoom shows one (PixelatorView::GridFromZoom)
    int tiles = 0;            // Tile grid every this many pixels; 0: none
    PixelatorBackground background = PixelatorBackground::Checker;
    bool pivot = true;
    std::optional<Ink> focus;
};
// The zoom a picture uses: `requested` (1-64) lowered until both sides fit PixelatorPictureMaxSide, or for 0 the
// largest whose long side stays within PixelatorPictureAutoSide (at least 1).
[[nodiscard]] int PixelatorPictureZoom(Vec2 sprite, int requested);
// What a picture shows besides the frame in its view's colours (D65).
struct PixelatorPictureLayer {
    const PixelatorImage* image = nullptr;
    std::vector<Color4> colors; // One per pixel of `image`
    Vec2 offset;                // Where its top-left corner lands in the frame's pixels
};
struct PixelatorPictureExtras {
    std::vector<PixelatorPictureLayer> under;  // Onion-skin ghosts, drawn first
    std::optional<std::vector<Color4>> colors; // Replace the view's colours (the Diff view)
    int repeat = 1;                            // The tile preview: the frame repeated this many times each way
    std::vector<Vec2> marks;                   // Pixels to ring (lint findings)
};
inline constexpr Color4 PixelatorMarkColor{255, 224, 0, 255};

// The sprite zoomed by `options.zoom` (already resolved, 1-64) on its background, with the grids and pivot marker.
// Throws for an empty sprite or a zoom outside 1-64.
[[nodiscard]] ViewportPixels ComposePixelatorPicture(const PixelatorImage& image, std::span<const Color4> palette,
                                                     const PixelatorPictureOptions& options,
                                                     const PixelatorPictureExtras& extras = {});
// `picture` in the top-left corner of a `width` x `height` one filled with the composed pictures' surround colour.
[[nodiscard]] ViewportPixels PadPixelatorPicture(const ViewportPixels& picture, int width, int height);
