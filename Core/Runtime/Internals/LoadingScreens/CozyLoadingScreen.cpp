#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

void DrawCozyLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    DrawLoadingStandIn(paint, frame, Rgb(0xf3e3cf), Rgb(0xc9825b));
}
