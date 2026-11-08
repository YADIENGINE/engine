#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

void DrawArcadeLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    DrawLoadingStandIn(paint, frame, Rgb(0x120a24), Rgb(0xff4fa3));
}
