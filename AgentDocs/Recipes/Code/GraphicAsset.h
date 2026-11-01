// [recipe:GraphicAsset-header]
#pragma once
#include "Utilities/FloatRect.h"

class Painter;

enum class GoblinKind { Forest, Ice };

void DrawGoblin(Painter& paint, const FloatRect& box, GoblinKind kind, float time);
// [/recipe]
