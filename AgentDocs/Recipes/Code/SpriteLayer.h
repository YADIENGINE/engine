#pragma once
#include "Utilities/Geometry/FloatRect.h"

class Painter;
class Surface;

void DrawRecipeLayer(Painter& paint, Surface& background, const FloatRect& destination, float opacity);
