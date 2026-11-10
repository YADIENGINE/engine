#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <vector>
#include "Editor/Internals/Session/FrameCapture.h"

// A palette GIF writer for pictures with few colours, such as pixel art: every colour is written exactly, with no
// quantising or dithering (D65). Frames share one size and one global colour table; pixels below half alpha are
// transparent; the animation loops forever. Delays are in hundredths of a second, one per frame.
inline constexpr std::size_t IndexedGifMaxColors = 255; // One table entry stays for transparency

// The GIF, or nothing when the frames use more than IndexedGifMaxColors opaque colours (another encoder must quantise
// them). Throws invalid_argument for no frames, frames of different sizes or a delay count that does not match.
[[nodiscard]] std::optional<std::vector<std::byte>> EncodeIndexedGif(std::span<const ViewportPixels> frames,
                                                                     std::span<const int> delays);
