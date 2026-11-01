#pragma once
#include <optional>
#include "Utilities/Float2.h"
#include "Utilities/Vec2.h"

struct SceneViewport {
    Float2 position;
    Float2 size;
    Vec2 canvas;
    Float2 clipPosition;
    Float2 clipSize;
};

[[nodiscard]] SceneViewport FitSceneViewport(Float2 position, Float2 available, Vec2 canvas);
[[nodiscard]] std::optional<Vec2> MapViewportPoint(const SceneViewport& viewport, Float2 point);
