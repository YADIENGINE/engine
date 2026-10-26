#pragma once
#include <optional>
#include "Utilities/Float2.h"
#include "Utilities/Vec2.h"

struct PreviewViewport {
    Float2 position;
    Float2 size;
    Vec2 canvas;
    Float2 clipPosition;
    Float2 clipSize;
};

[[nodiscard]] PreviewViewport FitPreviewViewport(Float2 position, Float2 available, Vec2 canvas);
[[nodiscard]] std::optional<Vec2> MapPreviewPoint(const PreviewViewport& viewport, Float2 point);
