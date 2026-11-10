#pragma once
#include <nlohmann/json.hpp>
#include <cstddef>
#include <string>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Editor/Internals/Pixelator/PixelatorPicture.h"

// The worker's "pixels" message, its answer to the editor's "pixels" request ({"id": catalog id}): a pixel graphic
// as data for the Pixelator (Docs/Design/README.md → D64). Inks and parts travel as base64 of one byte per pixel.
//   {"id", "error"?, "items": [{"name", "id"}], "item", "images": [{"size": [w, h], "pivot": [x, y], "inks", "parts"}],
//    "palettes": [["#rrggbbaa", ...]], "frames": [{"image", "palette", "seconds"}], "window", "looped", "capped"}
inline constexpr int PixelatorMaxSide = 4096; // Pixels per sprite side the editor accepts

[[nodiscard]] nlohmann::json EncodePixelatorAsset(const PixelatorAsset& asset);
// Throws for malformed data: sizes out of range, pixel counts that differ from the size, frames naming missing
// sprites or palettes.
[[nodiscard]] PixelatorAsset DecodePixelatorAsset(const nlohmann::json& json);
// What `ctl pixels` reports of a frame besides the picture: asset, item, items, size, pivot, frame, frames, seconds,
// looped, capped, inks (ink, color "#rrggbb[aa]", pixels) and parts (part, pixels).
[[nodiscard]] nlohmann::json PixelatorSummary(const PixelatorAsset& asset, std::size_t frame);
// "#rrggbb", with "aa" when not opaque.
[[nodiscard]] std::string PixelatorHex(Color4 color);

// What `ctl pixels` asks for: {"view": "color" | "inks" | "parts", "zoom": 1-64, "frame": N, "grid": bool,
// "tiles": 0-256, "background": "checker" | "dark" | "light", "pivot": bool, "ink": 1-255 (the others dimmed)},
// every key optional. Throws with a readable message for a value that does not fit.
struct PixelatorRequest {
    PixelatorPictureOptions picture;
    std::size_t frame = 0;
};
[[nodiscard]] PixelatorRequest DecodePixelatorRequest(const nlohmann::json& arguments);
[[nodiscard]] const char* PixelatorViewName(PixelatorViewMode view);
