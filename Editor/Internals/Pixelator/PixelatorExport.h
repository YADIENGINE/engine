#pragma once
#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Editor/Internals/Session/FrameCapture.h"

// Exporting a pixel graphic's frames as files (D65): one frame, a strip or grid of all frames as PNG, or an animated
// GIF, 1-8 times enlarged on a transparent background. Every frame sits in the box all frames share, aligned on its
// pivot, so the animation does not jump. Files go to the project's `Exports/<Graphic>/` folder.

enum class PixelatorExportKind { Frame, Strip, Grid, Gif };
inline constexpr int PixelatorExportMaxScale = 8;
inline constexpr int PixelatorExportMaxSide = 16384; // Pixels of an exported picture

struct PixelatorExportOptions {
    PixelatorExportKind kind = PixelatorExportKind::Frame;
    int scale = 1;          // 1-8
    std::size_t frame = 0;  // Frame
    int columns = 0;        // Grid: 0 is about square
    bool operator==(const PixelatorExportOptions&) const = default;
};

[[nodiscard]] const char* PixelatorExportKindName(PixelatorExportKind kind);
// Throws invalid_argument for a name that is not frame, strip, grid or gif.
[[nodiscard]] PixelatorExportKind PixelatorExportKindOf(const std::string& name);

// One frame in the shared box at `scale`, transparent around it. Throws for a frame or scale out of range.
[[nodiscard]] ViewportPixels ComposePixelatorFrame(const PixelatorAsset& asset, std::size_t frame, int scale);
// Every frame left to right in `columns` columns (frames.size() for a strip), rows top to bottom.
[[nodiscard]] ViewportPixels ComposePixelatorSheet(const PixelatorAsset& asset, int columns, int scale);
// Columns of a grid of `frames`: as given, or for 0 the smallest count that is at least the square root.
[[nodiscard]] int PixelatorSheetColumns(std::size_t frames, int columns);
// Each frame's GIF delay in hundredths of a second: rounded so the total stays the animation's length, at least 2
// (players show shorter delays as 10).
[[nodiscard]] std::vector<int> PixelatorGifDelays(const PixelatorAsset& asset);
// An animated GIF of every frame, looping forever; transparent where the frame is. Up to 255 colours (pixel art) every
// colour stays exact (EncodeIndexedGif); more go through msf_gif, which quantises and dithers. Throws when it cannot
// be encoded.
[[nodiscard]] std::vector<std::byte> EncodePixelatorGif(const PixelatorAsset& asset, int scale);

// The graphic's folder name and the file name an export gets: "Knight" and "Idle-frame3.png" (frames count from 1),
// "Idle-strip.png", "Idle-grid.png", "Idle.gif"; a single drawing uses the graphic's name.
[[nodiscard]] std::string PixelatorExportFolderName(const PixelatorAsset& asset);
[[nodiscard]] std::string PixelatorExportFileName(const PixelatorAsset& asset, const PixelatorExportOptions& options);
// `<project>/Exports/<graphic>`.
[[nodiscard]] std::filesystem::path PixelatorExportFolder(const std::filesystem::path& projectRoot,
                                                          const PixelatorAsset& asset);
// Composes and writes the export into its folder (made when missing) and returns the file. `encodePng` turns a
// picture into PNG bytes (the editor's encoder). Throws with a readable message when it cannot.
std::filesystem::path WritePixelatorExport(const std::filesystem::path& projectRoot, const PixelatorAsset& asset,
                                           const PixelatorExportOptions& options,
                                           const std::function<std::vector<std::byte>(const ViewportPixels&)>& encodePng);
