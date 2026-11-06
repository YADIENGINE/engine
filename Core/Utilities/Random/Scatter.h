#pragma once
#include <vector>
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"
#include "Utilities/Random/Rng.h"

// Points spread evenly over 'area' (Poisson disk sampling, Bridson's method): no two closer than 'spacing', and no gap
// left where another point would fit, so they look scattered by hand rather than clumped (trees, stars, pickups, spawn
// spots). Each point tries 'attempts' places around it before it is done; more fills the area more tightly but takes
// longer. The same area, spacing and generator state give the same points in the same order in every build of the same
// program; the code is built with precise floats, so a simulation may call it.
//     const std::vector<Float2> trees = ScatterPoints({0.0f, 0.0f, 1280.0f, 720.0f}, 48.0f, rng);
// Returns no points for an empty area or a spacing that is not above 0, or one so small for the area that over four
// million grid cells would be needed (a few million points).
[[nodiscard]] std::vector<Float2> ScatterPoints(const FloatRect& area, float spacing, Rng& rng, int attempts = 30);
