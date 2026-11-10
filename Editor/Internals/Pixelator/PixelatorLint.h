#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"
#include "Utilities/Geometry/Vec2.h"

// Pixel-art lint (D65): measurable flaws of a pixel graphic, said in words, for agents that cannot look and for
// people. Few, trustworthy findings: stair steps, dithering, speckle and deliberate single-pixel details (stars, eye
// glints) are not flaws. Rules:
//   stray-pixel     a lone pixel in a flat area of another ink, close to it in lightness (noise, not a detail)
//   outline-double  an outline corner that thickens the line: an L of outline pixels bending around nothing
//   outline-gap     a pixel of another ink where an outlined sprite touches the transparent outside
//   ramp-contrast   neighbouring tones of a registered ramp too close in Oklab lightness, or out of order; without
//                   ramps, two touching inks that look almost the same
//   unused-ink      palette inks no frame uses (info)
//   color-count     more colours than a sprite of this size keeps readable (info)
enum class PixelatorLintSeverity { Info, Warning };

struct PixelatorFinding {
    std::string rule;
    PixelatorLintSeverity severity = PixelatorLintSeverity::Warning;
    std::optional<std::size_t> image; // The distinct sprite; none for findings about the whole item
    std::vector<std::size_t> frames;  // Frames showing that sprite
    std::vector<Vec2> pixels;         // Where; may be empty
    std::vector<int> inks;
    std::string text;
    bool operator==(const PixelatorFinding&) const = default;
};

// Thresholds, tuned on hand-drawn and code-drawn sprites; tests change them.
struct PixelatorLintOptions {
    float strayContrast = 0.16f;  // A lone pixel at least this far (Oklab) from its surround is a deliberate detail
    int strayRepeats = 2;         // More lone pixels of one ink in one other ink are a texture (speckle, stars)
    float outlineShare = 0.75f;   // Of the edge pixels in one ink: the sprite has an outline
    int outlineMinEdge = 12;      // Edge pixels a sprite needs before its outline is judged
    int particleSize = 4;         // Loose groups of at most this many pixels (sparks, light) need no outline
    float rampStep = 0.035f;      // Least Oklab lightness between neighbouring ramp tones
    float similarLightness = 0.02f; // Without ramps: touching inks closer than this in lightness...
    float similarHue = 0.03f;       // ...and this in colour (Oklab a, b) look the same
    int similarEdges = 6;           // ...when they touch along at least this many pixel edges
    int maxPerRule = 24;            // Findings of one rule in one sprite; the last one counts the rest
};

[[nodiscard]] std::vector<PixelatorFinding> LintPixelator(const PixelatorAsset& asset,
                                                          const PixelatorLintOptions& options = {});
[[nodiscard]] const char* PixelatorSeverityName(PixelatorLintSeverity severity);
// Warnings among `findings`.
[[nodiscard]] std::size_t PixelatorWarnings(const std::vector<PixelatorFinding>& findings);
// The colour budget of a sprite: 8 colours up to 16 pixels on its long side, 16 up to 32, 24 up to 64, 32 up to 128,
// 48 above.
[[nodiscard]] int PixelatorColorBudget(int width, int height);
