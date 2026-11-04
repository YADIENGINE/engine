#pragma once
#include <cmath>
#include "Utilities/Float2.h"

// What effect styles and calls ask of their numbers.
[[nodiscard]] inline bool FxPositive(float value) { return std::isfinite(value) && value > 0.0f; }
[[nodiscard]] inline bool FxNotNegative(float value) { return std::isfinite(value) && value >= 0.0f; }
[[nodiscard]] inline bool FxFinite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
