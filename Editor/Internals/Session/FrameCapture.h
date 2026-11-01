#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Utilities/FloatRect.h"
#include "Utilities/Vec2.h"

// What a screenshot keeps of a preview frame: an optional crop in logical canvas units, then a uniform scale.
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
[[nodiscard]] PreviewPixels CaptureFrame(const PreviewPixels& frame, Vec2 canvas, const FrameCaptureOptions& options);
// A new file name in `directory` (time-stamped, never an existing one).
[[nodiscard]] std::filesystem::path NewCapturePath(const std::filesystem::path& directory);
// Deletes the oldest *.png files beyond `keep` in `directory`; ignores what cannot be removed.
void PruneCaptures(const std::filesystem::path& directory, std::size_t keep);
