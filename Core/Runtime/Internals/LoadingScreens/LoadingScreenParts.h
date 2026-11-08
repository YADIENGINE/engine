#pragma once
#include "Utilities/Color4.h"

class Painter;
struct LoadingScreenFrame;

// A stand-in picture: a coloured field and a sliding block, until a screen has its own drawing.
void DrawLoadingStandIn(Painter& paint, const LoadingScreenFrame& frame, Color4 background, Color4 mark);
