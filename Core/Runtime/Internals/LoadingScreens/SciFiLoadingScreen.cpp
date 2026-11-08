#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

void DrawSciFiLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    DrawLoadingStandIn(paint, frame, Rgb(0x070d18), Rgb(0x57c7ff));
}
