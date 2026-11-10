#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/Vec2.h"

// Where the Pixelator puts what it shows besides one frame: onion-skin ghosts, comparison panes, a 3x3 tile repeat and
// the box every frame of an animation fits in. Pure math, shared by the panel and `ctl pixels` (D65).

// Neighbouring frames shown faintly under the current one. Off by default; turned on it shows one frame each way.
struct PixelatorOnionOptions {
    int before = 1;         // Earlier frames, 0-3
    int after = 1;          // Later frames, 0-3
    float opacity = 0.4f;   // Of the nearest ghost; farther ones fade
    bool tinted = true;     // Earlier frames reddish, later bluish; false: their own colours
    bool operator==(const PixelatorOnionOptions&) const = default;
};
inline constexpr int PixelatorOnionMaxFrames = 3;
inline constexpr Color4 PixelatorOnionBefore{255, 72, 72, 255};
inline constexpr Color4 PixelatorOnionAfter{72, 150, 255, 255};

// One ghost: a frame, how far it is from the current one (-1 is the previous) and how it is drawn.
struct PixelatorGhost {
    std::size_t frame = 0;
    int distance = 0;
    float opacity = 0.0f;
    std::optional<Color4> tint;
    bool operator==(const PixelatorGhost&) const = default;
};
// The ghosts around `current`, farthest first (the order to draw them in). A looping animation wraps around its ends;
// one that does not stops there. The current frame and frames already shown are skipped.
[[nodiscard]] std::vector<PixelatorGhost> PixelatorOnionGhosts(std::size_t frameCount, std::size_t current, bool loops,
                                                               const PixelatorOnionOptions& options);
// A ghost's colour per pixel: its palette's colours (mixed halfway toward its tint when it has one) at its opacity.
[[nodiscard]] std::vector<Color4> PixelatorGhostColors(const PixelatorImage& image, std::span<const Color4> palette,
                                                       const PixelatorGhost& ghost);
// Where `other`'s top-left corner lands in `image`'s pixels when both are aligned on their pivots.
[[nodiscard]] Vec2 PixelatorPivotOffset(const PixelatorImage& image, const PixelatorImage& other);

// The smallest box every frame fits in when frames are aligned on their pivots, and where the pivot is inside it.
struct PixelatorFrameBox {
    Vec2 size;
    Vec2 pivot;
    bool operator==(const PixelatorFrameBox&) const = default;
};
[[nodiscard]] PixelatorFrameBox PixelatorFramesBox(const PixelatorAsset& asset);

// A comparison pane: its top-left corner and size inside the canvas area.
struct PixelatorPane {
    Float2 at;
    Float2 size;
    bool operator==(const PixelatorPane&) const = default;
};
inline constexpr int PixelatorMaxPanes = 4;
// `count` panes (1-4, clamped) filling `area` with `gap` between them: two or three side by side (stacked when the
// area is taller than wide), four in a 2x2 grid. Sizes are whole pixels.
[[nodiscard]] std::vector<PixelatorPane> PixelatorPanes(Float2 area, int count, float gap);

// The tile preview repeats a sprite this many times each way, so its seams show.
inline constexpr int PixelatorTileRepeat = 3;
// The sprite pixel a point of the repeated picture (in sprite pixels from its corner) shows.
[[nodiscard]] Vec2 PixelatorTiledPixel(Vec2 pixel, Vec2 sprite);
