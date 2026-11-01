#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Internals/Viewport/PreviewViewport.h"
#include "Utilities/FloatRect.h"

// Marking a region of the preview for the Console: the developer drags over (or clicks on) the image, the editor saves
// the region's pixels and pastes one context line for the agent running there (Docs/Editor/Automation.md → Marking).
inline constexpr float MarkPointSize = 160.0f;     // Canvas units of the square a click marks
inline constexpr float MarkClickPixels = 4.0f;     // A shorter drag (screen pixels) counts as a click
inline constexpr float MarkMaxImageSide = 1000.0f; // Larger regions are saved scaled down to this

// The canvas-space region of a drag from `start` to `end` (screen points), clamped to the canvas; a click marks a
// MarkPointSize square around it. Nothing when the press was outside the image.
[[nodiscard]] std::optional<FloatRect> MarkRegion(const PreviewViewport& viewport, Float2 start, Float2 end);
// The line pasted into the Console: preview, playback time, region, the saved image and the parameters that differ
// from their initial values. Ends with a space, ready for the developer's words; no newline.
[[nodiscard]] std::string MarkContext(const PreviewDefinition& definition, const PreviewParameters& values,
                                      double seconds, bool paused, FloatRect region, Vec2 canvas,
                                      const std::filesystem::path& image);
