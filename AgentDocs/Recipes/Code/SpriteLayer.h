#pragma once
#include "Utilities/FloatRect.h"

class Painter;
class Surface;

void DrawRecipeLayer(Painter& paint, Surface& background, const FloatRect& destination, float opacity);
