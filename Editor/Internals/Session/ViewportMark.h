#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Viewport/SceneViewport.h"
#include "Utilities/FloatRect.h"

// Marking a region of the scene for the Console: the developer drags over (or clicks on) the image, the editor saves
// the region's pixels and pastes one context line for the agent running there (Docs/Editor/Automation.md → Marking).
inline constexpr float MarkPointSize = 160.0f;       // Canvas units of the square a click marks
inline constexpr float MarkClickPixels = 4.0f;       // A shorter drag (screen pixels) counts as a click
inline constexpr float MarkMaxImageSide = 1000.0f;   // Larger regions are saved scaled down to this
inline constexpr float MarkWindowPointSize = 200.0f; // Window pixels of the square a click marks outside the scene

// The canvas-space region of a drag from `start` to `end` (screen points), clamped to the canvas; a click marks a
// MarkPointSize square around it. Nothing when the press was outside the image.
[[nodiscard]] std::optional<FloatRect> MarkRegion(const SceneViewport& viewport, Float2 start, Float2 end);
// A drag (or click) anywhere in the editor window as a rect in framebuffer pixels, clamped to the window. Points
// are UI coordinates; `scale` maps them to framebuffer pixels.
[[nodiscard]] std::optional<FloatRect> WindowMarkRegion(Float2 start, Float2 end, Float2 windowPosition,
                                                        Float2 windowSize, Float2 scale);
// The line pasted for a window mark; names the open scene, if any.
[[nodiscard]] std::string WindowMarkContext(FloatRect region, Vec2 window, const std::filesystem::path& image,
                                            const std::string& scene);
// The line pasted into the Console: scene, playback time, region, the saved image and the parameters that differ
// from their initial values. Ends with a space, ready for the developer's words; no newline.
[[nodiscard]] std::string MarkContext(const SceneDefinition& definition, const SceneParameters& values, double seconds,
                                      bool paused, FloatRect region, Vec2 canvas, const std::filesystem::path& image);
