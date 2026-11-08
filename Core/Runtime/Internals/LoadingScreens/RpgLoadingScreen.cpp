#include "Graphics/Paint/Painter.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenFrame.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreenParts.h"
#include "Runtime/Internals/LoadingScreens/LoadingScreens.h"

void DrawRpgLoadingScreen(Painter& paint, const LoadingScreenFrame& frame) {
    DrawLoadingStandIn(paint, frame, Rgb(0x14100c), Rgb(0xd9a441));
}
