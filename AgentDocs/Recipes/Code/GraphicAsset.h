// [recipe:GraphicAsset-header]
#pragma once
#include <optional>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

enum class GoblinKind { Forest, Ice };

// How the drawing looks on its own; a game changes only what it needs. The kind decides the skin unless a game names one.
struct GoblinLook {
    std::optional<Color4> skin;
    Color4 eye{16, 32, 48, 255};
};

void DrawGoblin(Painter& paint, const FloatRect& box, GoblinKind kind, float time, const GoblinLook& look = {});
// [/recipe]
