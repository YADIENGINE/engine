#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

void DrawHorrorLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    DrawLoadingStandIn(paint, frame, Rgb(0x050907), Rgb(0xbac9bf));
}
