#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Utilities/FloatRect.h"
#include "Utilities/Vec2.h"

// What a screenshot keeps of a viewport frame: an optional crop in logical canvas units, then a uniform scale.
struct FrameCaptureOptions {
    std::optional<FloatRect> crop;
    double scale = 1.0;
};

inline constexpr double FrameCaptureMinScale = 0.05;
inline constexpr double FrameCaptureMaxScale = 4.0;
inline constexpr std::size_t FrameCaptureKeep = 50; // Newest screenshots kept in the capture folder

// Crops (canvas units mapped onto the frame's pixels) and resamples: area average when shrinking, nearest pixel when
// enlarging.
// Throws when the crop leaves the canvas, is empty, or the scale is outside [FrameCaptureMinScale, MaxScale].
[[nodiscard]] ViewportPixels CaptureFrame(const ViewportPixels& frame, Vec2 canvas, const FrameCaptureOptions& options);
// Text the PNG encoder writes over an image, top-left at (x, y) in pixels.
struct FrameCaptureLabel {
    int x = 0;
    int y = 0;
    std::string text;
};
struct FrameGrid {
    ViewportPixels image;
    std::vector<FrameCaptureLabel> labels;
};
inline constexpr int FrameGridMaxFrames = 16;
inline constexpr int FrameGridGap = 4; // Pixels between tiles

// Equal-sized tiles in rows of `columns`, separated by FrameGridGap on a dark background; each label sits in its tile's
// top-left corner. Throws for no tiles, tiles of different sizes, or a label count that differs from the tiles.
[[nodiscard]] FrameGrid ComposeFrameGrid(const std::vector<ViewportPixels>& tiles,
                                         const std::vector<std::string>& labels, int columns);
// A new `<prefix>-<time>.png` in `directory`, never an existing one.
[[nodiscard]] std::filesystem::path NewCapturePath(const std::filesystem::path& directory, const std::string& prefix);
// Deletes the oldest *.png files beyond `keep` in `directory`; ignores what cannot be removed.
void PruneCaptures(const std::filesystem::path& directory, std::size_t keep);
